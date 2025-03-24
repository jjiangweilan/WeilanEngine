#include "Libs/AutoGrowLinearAllocator.hpp"
#include <gtest/gtest.h>

TEST(AutoGrowLinearAllocatorTest, InitialAllocation)
{
    AutoGrowLinearAllocator allocator(1024);
    EXPECT_EQ(allocator.GetSize(), 1024);
    EXPECT_EQ(allocator.GetMem() != nullptr, true);
}

TEST(AutoGrowLinearAllocatorTest, AllocateMemory)
{
    AutoGrowLinearAllocator allocator(1024);
    int* intPtr = allocator.Allocate<int>(1);
    EXPECT_NE(intPtr, nullptr);
    EXPECT_EQ(allocator.GetSize(), 1024);
}

TEST(AutoGrowLinearAllocatorTest, GrowMemory)
{
    AutoGrowLinearAllocator allocator(1024);
    allocator.Allocate<unsigned char>(1024);
    EXPECT_EQ(allocator.GetSize(), 1024);
    allocator.Allocate<unsigned char>(1);
    EXPECT_GT(allocator.GetSize(), 1024);
}

TEST(AutoGrowLinearAllocatorTest, AppendData)
{
    AutoGrowLinearAllocator allocator(1024);
    char data[512] = {0};
    allocator.Append(data, 512);
    EXPECT_EQ(allocator.GetSize(), 1024);
    allocator.Append(data, 1024);
    EXPECT_GT(allocator.GetSize(), 1024);
}

TEST(AutoGrowLinearAllocatorTest, ResetAllocator)
{
    AutoGrowLinearAllocator allocator(1024);
    allocator.Allocate<int>(1);
    allocator.Reset();
    EXPECT_EQ(allocator.GetSize(), 1024);
    EXPECT_EQ(allocator.Allocate<int>(1) != nullptr, true);
}
