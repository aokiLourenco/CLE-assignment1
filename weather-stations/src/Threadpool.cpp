#include "Threadpool.hpp"
#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>

Threadpool::Threadpool(int numThreads)
    : stopFlag(false), tasksLeft(0)
{
    for (int i = 0; i < numThreads; i++) {
        threads.emplace_back(&Threadpool::worker, this);
    }
}

Threadpool::~Threadpool() {
    stop();
}

void Threadpool::addTask(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(mutex);
        tasks.push(std::move(task));
        ++tasksLeft;
    }
    condition.notify_one();
}

void Threadpool::worker() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [this] { 
                return stopFlag || !tasks.empty(); 
            });
            if (stopFlag && tasks.empty())
                return;
            task = std::move(tasks.front());
            tasks.pop();
        }
        task();
        {
            std::unique_lock<std::mutex> lock(mutex);
            --tasksLeft;
            if (tasksLeft == 0)
                finishedCondition.notify_all();
        }
    }
}

void Threadpool::wait() {
    std::unique_lock<std::mutex> lock(mutex);
    finishedCondition.wait(lock, [this] { return tasksLeft == 0; });
}

void Threadpool::stop() {
    {
        std::unique_lock<std::mutex> lock(mutex);
        stopFlag = true;
    }
    condition.notify_all();
    for (auto &t : threads) {
        if (t.joinable())
            t.join();
    }
}