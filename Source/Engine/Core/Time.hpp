#pragma once
#include <chrono>

class Time
{
public:
    static void Tick();

    static float DeltaTime()
    {
        return DeltaTimeIntenral();
    }

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

    static float& DeltaTimeIntenral();

    static TimePoint lastTime;
};
