#pragma once
#include <chrono>
#include <iostream>
#include <stack>
#include <string>
#include <vector>

struct ProfileScope
{
    std::string label;
    std::chrono::time_point<std::chrono::nanoseconds> startTime;
    int64_t totalTime = 0;
    float GetMilliseconds() const { return totalTime * 1e-3f; }
    std::vector<ProfileScope> children;
};

class Profiler
{
public:
    inline static int MAX_FRAME_TRACKED = 800;

    Profiler() { frameProfiles.resize(MAX_FRAME_TRACKED); }
    bool IsPaused() const { return paused; }
    void Pause() const { paused = true; }
    void Resume() const { paused = false; }
    void Begin(std::string_view label);
    void End();

    void BeginFrame();
    void EndFrame();

    // timestamp in nanoseconds
    void BeginFrameManual(uint64_t timestamp);
    void EndFrameManual(uint64_t timestamp);
    void BeginManual(std::string_view label, uint64_t timestamp);
    void EndManual(uint64_t timestamp);

    const ProfileScope& GetLatestProfile() const
    {
        return frameProfiles[inProfiling ? currentFrame : currentFrame - 1];
    }

    std::vector<float> GetFlattendFrametime() const;
    int GetLatestFrameIndex() const { return inProfiling ? currentFrame : currentFrame - 1; }
    const std::vector<ProfileScope>& GetFrameProfiles() const { return frameProfiles; }
    int GetFrameIndex() const { return currentFrame; }
    int GetTrackCycles() const { return trackCycles; }

    static Profiler& GetSingleton();

private:
    mutable bool paused = false;
    bool actuallyPaused = false;
    bool inProfiling = false;
    std::stack<ProfileScope*> activeScopes;
    std::vector<ProfileScope> frameProfiles;
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
