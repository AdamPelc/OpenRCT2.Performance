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
        enum class Type {
            Task,
            Termination,
        };
        const std::function<bool()> WorkFn;
        Type type;

        explicit TaskData(std::function<bool()> workFn, Type type);
    };

    struct ThreadQueue
    {
        // Queue of tasks
        std::deque<TaskData> queue;
        std::atomic<bool> launchThread{false};
        std::atomic<bool> completedThread{false};
    };
    std::size_t _threadsAmount{};
    std::vector<std::thread> _threads;
    std::vector<ThreadQueue> _threadsQueues;

    std::size_t _tasksQueuesIdx{0};

    using unique_lock = std::unique_lock<std::mutex>;

public:
    explicit JobPoolPerf(size_t maxThreads = 255);
    ~JobPoolPerf();

    void AddTask(std::function<bool()> workFn);
    void AddTerminations();
    void Join();

private:
    void ProcessQueue(std::size_t threadIdx);
};
