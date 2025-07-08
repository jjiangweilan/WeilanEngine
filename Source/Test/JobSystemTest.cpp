#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include "Core/JobSystem.hpp"

// Test JobHandle: IsFinished and Wait
TEST(JobHandleTest, IsFinishedAndWait) {
    std::promise<void> p;
    auto f = p.get_future();
    JobHandle handle(std::move(f));
    EXPECT_FALSE(handle.IsFinished());
    p.set_value();
    handle.Wait();
    EXPECT_TRUE(handle.IsFinished());
}

// Test JobSystem: Scehdule and WaitAll
TEST(JobSystemTest, ScehduleAndWaitAll) {
    JobSystem js;
    std::atomic<int> counter = 0;
    auto job = js.Scehdule([&counter] { counter++; });
    job.Wait();
    EXPECT_EQ(counter, 1);
}

// Test JobSystem: Multiple Jobs
TEST(JobSystemTest, MultipleJobs) {
    JobSystem js;
    std::atomic<int> counter = 0;
    std::vector<JobHandle> jobs;
    for (int i = 0; i < 10; ++i) {
        jobs.push_back(js.Scehdule([&counter] { counter++; }));
    }
    for (auto& job : jobs) {
        job.Wait();
    }
    EXPECT_EQ(counter, 10);
}

// Test JobSystem: GetTotalWorkers
TEST(JobSystemTest, GetTotalWorkers) {
    int workers = JobSystem::GetTotalWorkers();
    EXPECT_GE(workers, 1);
}

// Test JobSystem: TryPopLocalJob and TryStealOtherJob (indirectly via Scehdule)
TEST(JobSystemTest, TryPopAndStealJob) {
    JobSystem js;
    std::atomic<int> counter = 0;
    std::vector<JobHandle> jobs;
    for (int i = 0; i < 20; ++i) {
        jobs.push_back(js.Scehdule([&counter] { counter++; }));
    }
    for (auto& job : jobs) {
        job.Wait();
    }
    EXPECT_EQ(counter, 20);
}
