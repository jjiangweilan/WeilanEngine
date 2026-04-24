#pragma once
#include <chrono>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <cstdint>

struct ProfileScope
{
    std::string label;
    std::chrono::time_point<std::chrono::nanoseconds> startTime;
    int64_t totalTime = 0;
    float GetMilliseconds() const { return totalTime * 1e-3f; }
    std::vector<std::unique_ptr<ProfileScope>> children;
};

class IProfiler
{
public:
    virtual ~IProfiler() = default;

    // Core profiling interface
    virtual bool IsPaused() const = 0;
    virtual void Pause() const = 0;
    virtual void Resume() const = 0;
    virtual void Begin(std::string_view label) = 0;
    virtual void End() = 0;

    // Frame profiling
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;

    // Manual timestamp profiling (timestamp in nanoseconds)
    virtual void BeginFrameManual(uint64_t timestamp) = 0;
    virtual void EndFrameManual(uint64_t timestamp) = 0;
    virtual void BeginManual(std::string_view label, uint64_t timestamp) = 0;
    virtual void EndManual(uint64_t timestamp) = 0;

    // Data retrieval
    virtual const ProfileScope& GetLatestProfile() const = 0;
    virtual std::vector<float> GetFlattendFrametime() const = 0;
    virtual int GetLatestFrameIndex() const = 0;
    virtual const std::vector<std::unique_ptr<ProfileScope>>& GetFrameProfiles() const = 0;
    virtual int GetFrameIndex() const = 0;
    virtual int GetTrackCycles() const = 0;
};