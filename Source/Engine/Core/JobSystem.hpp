#pragma once
#include "Engine/WeilanEngineAPI.hpp"
#include "Engine/Library/MPMCQueue.hpp"
#include <functional>
#include <future>
#include <queue>
#include <thread>

class JobHandle
{
    std::future<void> f;

public:
    JobHandle() : f() {}
    JobHandle(std::future<void>&& f) : f(std::move(f)) {}
    JobHandle(JobHandle&& other) noexcept : f(std::move(other.f)) {}
    bool IsFinished() { return f.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready; }
    bool IsValid() { return f.valid(); }
    void Wait()
    {
        if (f.valid())
            f.wait();
    }
    JobHandle& operator=(JobHandle&& other) noexcept
    {
        if (this != &other)
        {
            f = std::move(other.f);
        }
        return *this;
    }
};

class JobSystem
{
public:
    WEILAN_ENGINE_API JobSystem();
    WEILAN_ENGINE_API ~JobSystem();
    const int TotalWorkers = GetTotalWorkers();
    const int jobCapacityPerWorker = 256;
    WEILAN_ENGINE_API JobHandle Schedule(const std::function<void()>& f);
    WEILAN_ENGINE_API JobHandle Schedule(std::function<void()>&& f);
    void Execute();
    const std::thread::id& GetMainThreadID() { return mainThreadID; }
    void WaitAll();
    WEILAN_ENGINE_API static int GetTotalWorkers();
    WEILAN_ENGINE_API static void DeinitJobSystem();
    WEILAN_ENGINE_API static void InitJobSystem();
    WEILAN_ENGINE_API static JobSystem& Instance();

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
    JobHandle ScheduleInternal(std::packaged_task<void()>&& packed);
};

#define ASSERT_IS_MAIN_THREAD ASSERT(std::this_thread::get_id() == JobSystem::Instance().GetMainThreadID());
