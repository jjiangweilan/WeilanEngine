#include "Engine/Library/CommandStream.hpp"
#include <atomic>
#include <gtest/gtest.h>
#include <thread>

namespace
{
std::atomic<int> g_counter{0};

struct IncrementCmd
{
    int amount;
};

void IncrementHandler(void* ptr)
{
    IncrementCmd* cmd = (IncrementCmd*)ptr;
    g_counter += cmd->amount;
}
} // namespace

TEST(CommandStreamTest, ThreadSafeUsage)
{
    g_counter = 0;
    CommandStream<true> stream(128);

    std::thread t1([&]()
                   {
        for(int i=0; i<50; ++i)
            stream.Push(IncrementHandler, IncrementCmd{1}); });

    std::thread t2([&]()
                   {
        for(int i=0; i<50; ++i)
            stream.Push(IncrementHandler, IncrementCmd{1}); });

    t1.join();
    t2.join();

    stream.Execute();

    EXPECT_EQ(g_counter.load(), 100);
}
