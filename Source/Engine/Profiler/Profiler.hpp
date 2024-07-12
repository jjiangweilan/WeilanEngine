#pragma once
#include <chrono>
#include <iostream>
#include <stack>
#include <string>
#include <vector>

struct ProfileScope
{
    std::string label;
    std::chrono::high_resolution_clock::time_point startTime;
    int64_t totalTime = 0;
    float GetMilliseconds() const
    {
        return totalTime * 1e-3f;
    }
    std::vector<ProfileScope> children;
};

class Profiler
{
public:
    inline static int MAX_FRAME_TRACKED = 800;

    Profiler()
    {
        frameProfiles.resize(MAX_FRAME_TRACKED);
    }

    bool IsPaused()
    {
        return paused;
    }

    void Pause()
    {
        paused = true;
    }

    void Resume()
    {
        paused = false;
    }

    void Begin(std::string_view label)
    {
        if (actuallyPaused)
            return;
        auto now = std::chrono::high_resolution_clock::now();
        ProfileScope newScope{std::string(label), now, 0, {}};

        if (!activeScopes.empty())
        {
            activeScopes.top()->children.push_back(newScope);
            activeScopes.push(&activeScopes.top()->children.back());
        }
        else
        {
            frameProfiles[currentFrame] = newScope;
            activeScopes.push(&frameProfiles[currentFrame]);
        }
    }

    void End()
    {
        if (actuallyPaused)
            return;
        auto now = std::chrono::high_resolution_clock::now();

        if (activeScopes.empty())
        {
            std::cerr << "Error: End called without a matching Begin" << std::endl;
            return;
        }

        auto* scope = activeScopes.top();
        activeScopes.pop();
        scope->totalTime += std::chrono::duration_cast<std::chrono::microseconds>(now - scope->startTime).count();
    }

    void BeginFrame()
    {
        actuallyPaused = paused;
        if (actuallyPaused)
            return;
        inProfiling = true;
        currentFrameStart = std::chrono::high_resolution_clock::now();
        while (!activeScopes.empty())
        {
            activeScopes.pop();
        }
        Begin("Root");
    }

    void EndFrame()
    {
        if (actuallyPaused)
            return;

        End();

        currentFrame = (currentFrame + 1) % MAX_FRAME_TRACKED;
        if (currentFrame == 0)
            trackCycles += 1;
        inProfiling = false;
    }

    const ProfileScope& GetLatestProfile() const
    {
        return frameProfiles[inProfiling ? currentFrame : currentFrame - 1];
    }

    int GetLatestFrameIndex()
    {
        return inProfiling ? currentFrame : currentFrame - 1;
    }

    const std::vector<ProfileScope>& GetFrameProfiles() const
    {
        return frameProfiles;
    }

    int GetFrameIndex() const
    {
        return currentFrame;
    }

    int GetTrackCycles() const
    {
        return trackCycles;
    }

    static Profiler& GetSingleton();

private:
    bool paused = false;
    bool actuallyPaused = false;
    bool inProfiling = false;
    std::stack<ProfileScope*> activeScopes;
    std::vector<ProfileScope> frameProfiles;
    int currentFrame = 0;
    int trackCycles = 0;
    std::chrono::high_resolution_clock::time_point currentFrameStart;
};

#define ENGINE_BEGIN_PROFILE(scopeName) Profiler::GetSingleton().Begin(scopeName);
#define ENGINE_END_PROFILE Profiler::GetSingleton().End();

#define ENGINE_BEGIN_FRAME_PROFILE Profiler::GetSingleton().BeginFrame();
#define ENGINE_END_FRAME_PROFILE Profiler::GetSingleton().EndFrame();
