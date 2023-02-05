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

JobPoolPerf::TaskData::TaskData(std::function<bool()> workFn, Type type)
    : WorkFn(std::move(workFn))
    , type{type}
{
}

JobPoolPerf::JobPoolPerf(size_t maxThreads)
    : _threadsAmount{std::min<std::size_t>(maxThreads, std::thread::hardware_concurrency())}
    , _threadsQueues(_threadsAmount)
{
    for (size_t threadIdx = 0; threadIdx < _threadsAmount; threadIdx++)
    {
        _threads.emplace_back(&JobPoolPerf::ProcessQueue, this, threadIdx);
    }
}

JobPoolPerf::~JobPoolPerf()
{
    AddTerminations();
    Join();
    for (auto& th : _threads)
    {
        assert(th.joinable() != false);
        th.join();
    }
}

void JobPoolPerf::AddTask(std::function<bool()> workFn)
{
    if(_tasksQueuesIdx == _threadsAmount)
    {
        _tasksQueuesIdx = 0;
    }

    _threadsQueues.at(_tasksQueuesIdx).queue.emplace_back(std::move(workFn), TaskData::Type::Task);
    _tasksQueuesIdx++;
}

void JobPoolPerf::AddTerminations()
{
    for(auto& queue : _threadsQueues) {
        queue.queue.emplace_back([]{ return true; }, TaskData::Type::Termination);
    }
}

void JobPoolPerf::Join()
{
    // Start threads
    for (auto& threadQueue : _threadsQueues) {
        threadQueue.launchThread = true;
    }

    for (auto& threadQueue : _threadsQueues) {
        while (!threadQueue.completedThread.exchange(false)) {
            static const timespec ns = { 0, 1 };
            nanosleep(&ns, nullptr);
        }
    }
}

void JobPoolPerf::ProcessQueue(std::size_t threadIdx)
{
    auto& threadQueue = _threadsQueues.at(threadIdx);
    bool should_stop = false;
    while (!should_stop) {
        // Wait for signal to start processing
        while (!threadQueue.launchThread.exchange(false)) {
            static const timespec ns = { 0, 1 };
            nanosleep(&ns, nullptr);
        }

        // Process all elements in queue
        while (!threadQueue.queue.empty()) {
            auto taskData = threadQueue.queue.front();
            threadQueue.queue.pop_front();
            should_stop = taskData.WorkFn();
        }

        // Notify that processing is done
        threadQueue.completedThread = true;
    }
}
