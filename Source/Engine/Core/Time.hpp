#pragma once
#include <chrono>

class Time
{
public:
    static void Tick();

    static float DeltaTime()
    {
        return GetTimeInternal().deltaTime;
    }

    static float TimeSinceLuanch()
    {
        return GetTimeInternal().timeSinceLuanch;
    };

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

    Time();

    static Time& GetTimeInternal();

    TimePoint lastTime;
    TimePoint launchTime;

    float timeSinceLuanch;
    float deltaTime;
};
