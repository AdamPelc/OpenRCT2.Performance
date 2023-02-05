/*****************************************************************************
 * Copyright (c) 2014-2023 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "JobPoolPerf.h"

#include <algorithm>
#include <cassert>
#include <utility>

JobPoolPerf::TaskData::TaskData(std::function<void()> workFn)
    : WorkFn(std::move(workFn))
{
}

JobPoolPerf::JobPoolPerf(size_t maxThreads)
    : _threadsAmount{std::min<std::size_t>(maxThreads, std::thread::hardware_concurrency())}
    , _processing(_threadsAmount)
    , _pendingsQues(_threadsAmount)
    , _completedQues(_threadsAmount)
    , _condsPending(_threadsAmount)
    , _condsComplete(_threadsAmount)
    , _mutexes(_threadsAmount)
{
    for (size_t threadIdx = 0; threadIdx < _threadsAmount; threadIdx++)
    {
        _threads.emplace_back(&JobPoolPerf::ProcessQueue, this, threadIdx);
    }
}

JobPoolPerf::~JobPoolPerf()
{
    _shouldStop = true;

    for (auto threadIdx = 0U; threadIdx != _threadsAmount; ++threadIdx)
    {
        unique_lock lock(_mutexes.at(threadIdx));
        _condsPending.at(threadIdx).notify_all();
    }

    for (auto& th : _threads)
    {
        assert(th.joinable() != false);
        th.join();
    }
}

void JobPoolPerf::AddTask(std::function<void()> workFn)
{
    if(_threadIdx == _threadsAmount)
    {
        _threadIdx = 0;
    }

    std::scoped_lock lock(_mutexes.at(_threadIdx));
    _pendingsQues.at(_threadIdx).emplace_back(std::move(workFn));
    _condsPending.at(_threadIdx).notify_one();

    _threadIdx++;
}

void JobPoolPerf::Join()
{
    for (auto threadIdx = 0U; threadIdx != _threadsAmount; ++ threadIdx)
    {
        unique_lock lock(_mutexes.at(threadIdx));
        while (true)
        {
            // Wait for the queue to become empty or having completed tasks.
            _condsComplete.at(threadIdx).wait(lock,
              [this, threadIdx]() { return (_pendingsQues.at(threadIdx).empty() && _processing.at(threadIdx) == 0) || !_completedQues.at(threadIdx).empty(); });

            // Dispatch all completion callbacks if there are any.
            while (!_completedQues.at(threadIdx).empty())
            {
                auto taskData = _completedQues.at(threadIdx).front();
                _completedQues.at(threadIdx).pop_front();
            }

            // If everything is empty and no more work has to be done we can stop waiting.
            if (_completedQues.at(threadIdx).empty() && _pendingsQues.at(threadIdx).empty() && _processing.at(threadIdx) == 0)
            {
                break;
            }
        }
    }
}

void JobPoolPerf::ProcessQueue(std::size_t threadIdx)
{
    unique_lock lock(_mutexes.at(threadIdx));
    do
    {
        // Wait for work or cancellation.
        _condsPending.at(threadIdx).wait(lock, [this, threadIdx]() { return _shouldStop || !_pendingsQues.at(threadIdx).empty(); });

        if (!_pendingsQues.at(threadIdx).empty())
        {
            _processing.at(threadIdx)++;

            auto taskData = _pendingsQues.at(threadIdx).front();
            _pendingsQues.at(threadIdx).pop_front();

            lock.unlock();

            taskData.WorkFn();

            lock.lock();

            _completedQues.at(threadIdx).push_back(std::move(taskData));

            _processing.at(threadIdx)--;
            _condsComplete.at(threadIdx).notify_one();
        }
    } while (!_shouldStop);
}
