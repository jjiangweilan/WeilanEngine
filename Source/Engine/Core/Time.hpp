#pragma once
#include <chrono>

namespace Engine
{
class Time
{
public:
    static void Tick();

    static float DeltaTime()
    {
        return deltaTime;
    }

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

    static TimePoint lastTime;
    static float deltaTime;
};
} // namespace Engine
