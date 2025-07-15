#include "JobSystem.hpp"

JobSystem::JobSystem() : mainThreadJobs(jobCapacityPerWorker), done(false)
{
    mainThreadID = std::this_thread::get_id();
    for (int i = 0; i < TotalWorkers; i++)
    {
        workerJobs.push_back(std::make_unique<JobQueue>(jobCapacityPerWorker));
    }

    for (int i = 0; i < TotalWorkers; i++)
    {
        workers.push_back(std::make_unique<std::thread>(
            [this, i]()
            {
                currentWorkerIdx = i;
                this->WorkerThread(i);
                currentWorkerIdx = -1;
            }
        ));
    }
}

JobSystem::~JobSystem()
{
    done = true;
    workerSignal.notify_all();
    for (auto& w : workers)
    {
        if (w->joinable())
            w->join();
    }
}

// Definition for static thread_local member
thread_local int JobSystem::currentWorkerIdx = -1;

int JobSystem::GetTotalWorkers()
{
    int totalWorkers = std::thread::hardware_concurrency();
    if (totalWorkers == 0)
    {
        totalWorkers = 6;
    }

    totalWorkers -= 1; // Reserve one thread for main thread

    return totalWorkers;
}

JobHandle JobSystem::Schedule(const std::function<void()>& f)
{
    auto packed = std::packaged_task<void()>(f);
    auto future = packed.get_future();

    // Select the job queue to schedule the job
    JobQueue* scheduleTo = nullptr;
    if (std::this_thread::get_id() == mainThreadID)
        scheduleTo = &mainThreadJobs;
    else
        scheduleTo = workerJobs[currentWorkerIdx].get();

    scheduleTo->push(std::move(packed));

    // Notify any thread to run
    workerSignal.notify_one();

    return JobHandle(std::move(future));
}

bool JobSystem::TryPopLocalJob(Job& f)
{
    return workerJobs[currentWorkerIdx]->try_pop(f); // Do I need to test empty before using try_pop()?
}

void JobSystem::WorkerThread(int threadIdx)
{
    while (!done)
    {
        Job job;
        // Try work on a job
        // 1. Try getting from local
        // 2. Try getting from main thread
        // 3. Try stealing from other workers
        if (TryPopLocalJob(job) || TryPopMainThreadJob(job) || TryStealOtherJob(job))
            job();
        else
        {
            std::unique_lock lk{workerSignalMutex};
            workerSignal.wait(lk);
        }
    }
}

bool JobSystem::TryStealOtherJob(Job& f)
{
    // Steal from next job queue to avoid thread contension
    for (int workerIdx = (currentWorkerIdx + 1) % TotalWorkers; workerIdx != currentWorkerIdx;
         workerIdx = (workerIdx + 1) % TotalWorkers)

    {
        if (workerJobs[workerIdx]->try_pop(f))
            return true;
    }

    // No job to steal
    return false;
}

bool JobSystem::TryPopMainThreadJob(Job& f)
{
    return mainThreadJobs.try_pop(f);
}

void JobSystem::InitJobSystem()
{
    if (instance == nullptr)
    {
        instance = std::unique_ptr<JobSystem>(new JobSystem());
    }
}

JobSystem& JobSystem::Instance()
{
    return *instance;
}

void JobSystem::DeinitJobSystem()
{
    instance = nullptr;
}

std::unique_ptr<JobSystem> JobSystem::instance = nullptr;
