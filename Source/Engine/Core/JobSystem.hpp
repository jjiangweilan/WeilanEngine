#pragma once
#include "Libs/MPMCQueue.hpp"
#include <functional>
#include <future>
#include <queue>
#include <thread>

class JobHandle
{
public:
    JobHandle() : f() {}
    JobHandle(std::future<void>&& f) : f(std::move(f)) {}
    bool IsFinished() { return f.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }
    void Wait()
    {
        if (f.valid())
            f.wait();
    }

private:
    std::future<void> f;
};

class JobSystem
{
public:
    JobSystem();
    ~JobSystem();
    const int TotalWorkers = GetTotalWorkers();
    const int jobCapacityPerWorker = 256;
    JobHandle Schedule(const std::function<void()>& f);
    void Execute();
    const std::thread::id& GetMainThreadID() { return mainThreadID; }
    void WaitAll();
    static int GetTotalWorkers();
    static void DeinitJobSystem();
    static void InitJobSystem();
    static JobSystem& Instance();

private:
    static std::unique_ptr<JobSystem> instance;

    using Job = std::packaged_task<void()>;
    using JobQueue = MPMCQueue<Job>;

    static int thread_local currentWorkerIdx;

    std::atomic_bool done;
    std::thread::id mainThreadID;
    std::vector<std::unique_ptr<std::thread>> workers{};
    std::vector<std::unique_ptr<JobQueue>> workerJobs{};
    std::condition_variable workerSignal{};
    std::mutex workerSignalMutex{};
    JobQueue mainThreadJobs;

    bool TryPopLocalJob(Job& f);
    bool TryPopMainThreadJob(Job& f);
    bool TryStealOtherJob(Job& f);
    void WorkerThread(int workerIdx);
};
