#ifndef THREADPOOL_HPP
#define THREADPOOL_HPP

#include <vector>
#include <queue>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <string>

struct WSData {
    std::string name;
    float sum;
    float average;
    float min;
    float max;
    int count = 0;
};

class Threadpool {
public:
    Threadpool(int numThreads);
    ~Threadpool();
    void addTask(std::function<void()> task);
    void wait();  
    void stop();
private:
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::mutex mutex;
    std::condition_variable condition;
    std::condition_variable finishedCondition;
    bool stopFlag;
    int tasksLeft;

    void worker();
};

#endif