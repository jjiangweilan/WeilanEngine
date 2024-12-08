#pragma once
#include <chrono>
#include <spdlog/spdlog.h>

class ScopedProfiler
{
public:
    ScopedProfiler(std::string&& message) : message(std::move(message))
    {
        begin = std::chrono::high_resolution_clock::now();
    }

    ~ScopedProfiler()
    {
        float pass =
            std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - begin)
                .count() *
            1e-3;

        if (!message.empty())
        {
            spdlog::info("Profiled \({}\), {}", pass, message);
        }
    }

private:
    std::string message;
    std::chrono::high_resolution_clock::time_point begin;
};

#ifndef ENGINE_DEV_BUILD
#define SCOPED_PROFILER(message)
#else
#define SCOPED_PROFILER(message) ScopedProfiler scoped_profiler_macro(message);
#endif
