#include "Time.hpp"
#include <spdlog/spdlog.h>

void Time::Tick()
{
    auto& t = GetTimeInternal();
    auto nowTime = Time::Clock::now();
    auto deltaTimeDuration = nowTime - t.lastTime;
    t.deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(deltaTimeDuration).count() * 1e-6f;

    t.timeSinceLuanch = std::chrono::duration_cast<std::chrono::microseconds>(nowTime - t.launchTime).count() * 1e-6f;

    t.lastTime = nowTime;
}

Time& Time::GetTimeInternal()
{
    static std::unique_ptr<Time> time = std::unique_ptr<Time>(new Time);
    return *time;
}

Time::Time()
{
    lastTime = Time::Clock::now();
    launchTime = Time::Clock::now();
}
