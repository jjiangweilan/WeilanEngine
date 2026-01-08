#pragma once

#include "IProfiler.hpp"
#include <chrono>
#include <iostream>
#include <memory>
#include <stack>
#include <string>

class Profiler : public IProfiler
{
public:
    inline static int MAX_FRAME_TRACKED = 800;

    Profiler() { frameProfiles.resize(MAX_FRAME_TRACKED); }
    bool IsPaused() const override { return paused; }
    void Pause() const override { paused = true; }
    void Resume() const override { paused = false; }
    void Begin(std::string_view label) override;
    void End() override;

    void BeginFrame() override;
    void EndFrame() override;

    // timestamp in nanoseconds
    void BeginFrameManual(uint64_t timestamp) override;
    void EndFrameManual(uint64_t timestamp) override;
    void BeginManual(std::string_view label, uint64_t timestamp) override;
    void EndManual(uint64_t timestamp) override;

    const ProfileScope& GetLatestProfile() const override
    {
        return *frameProfiles[inProfiling ? currentFrame : currentFrame - 1];
    }

    std::vector<float> GetFlattendFrametime() const override;
    int GetLatestFrameIndex() const override { return inProfiling ? currentFrame : currentFrame - 1; }
    const std::vector<std::unique_ptr<ProfileScope>>& GetFrameProfiles() const override { return frameProfiles; }
    int GetFrameIndex() const override { return currentFrame; }
    int GetTrackCycles() const override { return trackCycles; }

    static IProfiler& GetSingleton();

private:
    mutable bool paused = true;
    bool actuallyPaused = false;
    bool inProfiling = false;
    std::stack<ProfileScope*> activeScopes;
    std::vector<std::unique_ptr<ProfileScope>> frameProfiles;
    int currentFrame = 0;
    int trackCycles = 0;
};

struct ScopedProfile
{
    ScopedProfile(std::string_view label) { Profiler::GetSingleton().Begin(label); }
    ~ScopedProfile() { Profiler::GetSingleton().End(); }
};

#define ENGINE_SCOPED_PROFILE(label) ScopedProfile _engine_scopedProfile(label);

#define ENGINE_BEGIN_PROFILE(scopeName) Profiler::GetSingleton().Begin(scopeName);
#define ENGINE_END_PROFILE Profiler::GetSingleton().End();

#define ENGINE_BEGIN_FRAME_PROFILE Profiler::GetSingleton().BeginFrame();
#define ENGINE_END_FRAME_PROFILE Profiler::GetSingleton().EndFrame();
