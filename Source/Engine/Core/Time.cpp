#include "Time.hpp"
#include <spdlog/spdlog.h>
Time::TimePoint Time::lastTime = Time::Clock::now();

void Time::Tick()
{
    auto nowTime = Time::Clock::now();
    auto deltaTimeDuration = nowTime - lastTime;
    DeltaTimeIntenral() = std::chrono::duration_cast<std::chrono::microseconds>(deltaTimeDuration).count() * 1e-6f;
    lastTime = nowTime;
}

float& Time::DeltaTimeIntenral()
{
    static float deltaTime;
    return deltaTime;
}
