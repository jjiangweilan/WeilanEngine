#pragma once
#include "Libs/MPMCQueue.hpp"
#include <functional>
#include <future>
#include <queue>
#include <thread>

class JobHandle
{
public:
    JobHandle(std::future<void>&& f) : f(std::move(f)) {}
    bool IsFinished() { return f.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }
    void Wait() { f.wait(); }

private:
    std::future<void> f;
};

class JobSystem
{
public:
    const int TotalWorkers = GetTotalWorkers();
    const int jobCapacityPerWorker = 256;

    JobSystem();
    ~JobSystem();
    JobHandle Scehdule(const std::function<void()>& f);

    void WaitAll();

    static int GetTotalWorkers();

private:
    using Job = std::packaged_task<void()>;
    using JobQueue = MPMCQueue<Job>;

    static int thread_local currentWorkerIdx;

    std::atomic_bool done;
    std::thread::id mainThreadID;
    std::vector<std::unique_ptr<std::thread>> workers{};
    std::vector<std::unique_ptr<JobQueue>> workerJobs{};
    JobQueue mainThreadJobs;

    bool TryPopLocalJob(Job& f);
    bool TryPopMainThreadJob(Job& f);
    bool TryStealOtherJob(Job& f);
    void WorkerThread(int workerIdx);
};
