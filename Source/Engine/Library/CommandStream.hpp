#pragma once
#include <cstdint>
#include <functional>
#include <vector>

class CommandStream
{
public:
    void Push(const std::function<void()>& fn)
    {
    }

    void Execute()
    {
        for (auto& f : commandBuffers)
        {
            f();
        }

        commandBuffers.clear();
    }

    std::vector<std::function<void()>> commandBuffers;
};
