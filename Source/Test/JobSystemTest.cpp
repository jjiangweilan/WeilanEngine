#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include "Core/JobSystem.hpp"

// Test fixture for JobSystem tests
class JobSystemTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        jobSystem = std::make_unique<JobSystem>();
    }

    void TearDown() override {
        jobSystem.reset();
    }   

    std::unique_ptr<JobSystem> jobSystem;
};

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
TEST_F(JobSystemTestFixture, ScehduleAndWaitAll) {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::atomic<int> counter = 0;
    auto job = jobSystem->Scehdule([&counter] { counter++; });
    job.Wait();
    EXPECT_EQ(counter, 1);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Test execution time: " << duration.count() << " milliseconds" << std::endl;
}

// Test JobSystem: Multiple Jobs
TEST_F(JobSystemTestFixture, MultipleJobs) {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::atomic<int> counter = 0;
    std::vector<JobHandle> jobs;
    for (int i = 0; i < 10; ++i) {
        jobs.push_back(jobSystem->Scehdule([&counter] { counter++; }));
    }
    for (auto& job : jobs) {
        job.Wait();
    }
    EXPECT_EQ(counter, 10);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Test execution time: " << duration.count() << " milliseconds" << std::endl;
}

// Test JobSystem: GetTotalWorkers
TEST(JobSystemTest, GetTotalWorkers) {
    int workers = JobSystem::GetTotalWorkers();
    EXPECT_GE(workers, 1);
}

// Test JobSystem: TryPopLocalJob and TryStealOtherJob (indirectly via Scehdule)
TEST_F(JobSystemTestFixture, TryPopAndStealJob) {
    auto start = std::chrono::high_resolution_clock::now();
    
    int counter = 0;
    std::vector<JobHandle> jobs;
    for (int i = 0; i < 20; ++i) {
        jobs.push_back(jobSystem->Scehdule([&counter] { counter++; }));
    }
    for (auto& job : jobs) {
        job.Wait();
    }
    EXPECT_EQ(counter, 20);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Test execution time: " << duration.count() << " milliseconds" << std::endl;
}

// Test JobSystem: Nested Job Scheduling
TEST_F(JobSystemTestFixture, NestedJobScheduling) {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::atomic<int> counter = 0;
    std::vector<JobHandle> parentJobs;
    std::vector<JobHandle> childJobs;
    std::mutex lk;
    
    // Create parent jobs that will schedule child jobs
    for (int i = 0; i < 5; ++i) {
        parentJobs.push_back(jobSystem->Scehdule([this, &counter, &childJobs, &lk, i] {
            // Each parent job schedules 2 child jobs
            for (int j = 0; j < 2; ++j) {
                std::lock_guard l(lk);
                childJobs.push_back(jobSystem->Scehdule([&counter] {
                    counter++;
                }));
            }
            // Parent job also increments counter
            counter++;
        }));
    }
    
    // Wait for all parent jobs to complete
    for (auto& job : parentJobs) {
        job.Wait();
    }
    
    // Wait for all child jobs to complete
    for (auto& job : childJobs) {
        job.Wait();
    }
    
    // Verify that all jobs executed: 5 parent jobs + (5 * 2) child jobs = 15 total
    EXPECT_EQ(counter, 15);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Test execution time: " << duration.count() << " milliseconds" << std::endl;
}
