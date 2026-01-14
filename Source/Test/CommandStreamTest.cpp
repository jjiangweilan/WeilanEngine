#include "Engine/Library/CommandStream.hpp"
#include <gtest/gtest.h>

namespace
{

struct AddCmd
{
    int value;
};

struct MulCmd
{
    int value;
};

static int g_result = 0;

void AddHandler(void* ptr)
{
    AddCmd* cmd = (AddCmd*)ptr;
    g_result += cmd->value;
}

void MulHandler(void* ptr)
{
    MulCmd* cmd = (MulCmd*)ptr;
    g_result *= cmd->value;
}

} // namespace

TEST(CommandStreamTest, BasicUsage)
{
    g_result = 10;
    CommandStream stream;

    stream.Push(AddHandler, AddCmd{5}); // 10 + 5 = 15
    stream.Push(MulHandler, MulCmd{2}); // 15 * 2 = 30

    stream.Execute();

    EXPECT_EQ(g_result, 30);
    EXPECT_TRUE(stream.commandBuffer.empty());
}

TEST(CommandStreamTest, AlignmentCheck)
{
    g_result = 0;
    CommandStream stream;

    struct ByteCmd
    {
        char c;
    };
    // Captureless lambda converts to function pointer
    void (*ByteHandler)(void*) = [](void* ptr)
    {
        g_result++;
    };

    stream.Push(ByteHandler, ByteCmd{'a'});
    stream.Push(ByteHandler, ByteCmd{'b'});

    stream.Execute();
    EXPECT_EQ(g_result, 2);
}
