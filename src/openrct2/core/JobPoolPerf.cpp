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
    , _tasksQueues(_threadsAmount)
    , _condsLaunchThread(_threadsAmount)
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
        _condsLaunchThread.at(threadIdx).notify_one();
    }

    for (auto& th : _threads)
    {
        assert(th.joinable() != false);
        th.join();
    }
}

void JobPoolPerf::AddTask(std::function<void()> workFn)
{
    if(_tasksQueuesIdx == _threadsAmount)
    {
        _tasksQueuesIdx = 0;
    }
    _tasksQueues.at(_tasksQueuesIdx).emplace_back(std::move(workFn));
    _tasksLeft++;

    _tasksQueuesIdx++;
}

void JobPoolPerf::Join()
{
    // Start threads
    for (auto threadIdx = 0U; threadIdx < _threadsAmount; ++threadIdx)
    {
        std::scoped_lock lock(_mutexes.at(threadIdx));
        _condsLaunchThread.at(threadIdx).notify_one();
    }

    while (0 != _tasksLeft) {}
}

void JobPoolPerf::ProcessQueue(std::size_t threadIdx)
{
    unique_lock lock(_mutexes.at(threadIdx));
    do
    {
        _condsLaunchThread.at(threadIdx).wait(lock);
        while (!_tasksQueues.at(threadIdx).empty())
        {
            auto taskData = _tasksQueues.at(threadIdx).front();
            _tasksQueues.at(threadIdx).pop_front();

            taskData.WorkFn();
            _tasksLeft--;
        }
        _threadCompleted.notify_one();
    } while (!_shouldStop);
}
