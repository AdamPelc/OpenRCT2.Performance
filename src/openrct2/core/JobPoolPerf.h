/*****************************************************************************
 * Copyright (c) 2014-2023 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

class JobPoolPerf
{
private:
    struct TaskData
    {
        const std::function<void()> WorkFn;

        explicit TaskData(std::function<void()> workFn);
    };
    std::size_t _threadsAmount{};
    std::atomic_bool _shouldStop = { false };
    std::vector<std::thread> _threads;

    std::size_t _tasksQueuesIdx{0};
    std::vector<std::deque<TaskData>> _tasksQueues;
    std::atomic<std::size_t> _tasksLeft{0};

    // TODO: This probably might be single std::condition_variable
    std::vector<std::condition_variable> _condsLaunchThread;
    std::condition_variable _threadCompleted;

    std::vector<std::mutex> _mutexes;

    using unique_lock = std::unique_lock<std::mutex>;

public:
    explicit JobPoolPerf(size_t maxThreads = 255);
    ~JobPoolPerf();

    void AddTask(std::function<void()> workFn);
    void Join();

private:
    void ProcessQueue(std::size_t threadIdx);
};
