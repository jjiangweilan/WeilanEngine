#include "Profiler.hpp"

Profiler& Profiler::GetSingleton()
{
    static Profiler profiler;
    return profiler;
}

std::vector<float> Profiler::GetFlattendFrametime() const
{
    std::vector<float> frameTimes(Profiler::MAX_FRAME_TRACKED, 0);
    int oldestFrameIndex = (GetLatestFrameIndex() + 1) % Profiler::MAX_FRAME_TRACKED; // get oldest index
    if (GetTrackCycles() == 0) [[unlikely]]
    {
        for (int i = 0; i < oldestFrameIndex; i++)
        {
            auto& rootScopeProfile = frameProfiles[i];
            frameTimes[i] = rootScopeProfile->GetMilliseconds();
        }
    }
    else
    {
        for (int i = 0; i < Profiler::MAX_FRAME_TRACKED; i++)
        {
            auto& rootScopeProfile = frameProfiles[oldestFrameIndex];
            frameTimes[i] = rootScopeProfile->GetMilliseconds();
            oldestFrameIndex++;
            oldestFrameIndex %= Profiler::MAX_FRAME_TRACKED;
        }
    }

    return frameTimes;
}

void Profiler::Begin(std::string_view label)
{
    BeginManual(label, std::chrono::high_resolution_clock::now().time_since_epoch().count());
}

void Profiler::End()
{
    EndManual(std::chrono::high_resolution_clock::now().time_since_epoch().count());
}

void Profiler::BeginManual(std::string_view label, uint64_t timestamp)
{
    if (actuallyPaused)
        return;
    auto now = std::chrono::time_point<std::chrono::nanoseconds>(std::chrono::nanoseconds(timestamp));
    std::unique_ptr<ProfileScope> newScope = std::make_unique<ProfileScope>(ProfileScope(std::string(label), now, 0, {}));

    if (!activeScopes.empty())
    {
        activeScopes.top()->children.push_back(std::move(newScope));
        activeScopes.push(activeScopes.top()->children.back().get());
    }
    else
    {
        frameProfiles[currentFrame] = std::move(newScope);
        activeScopes.push(frameProfiles[currentFrame].get());
    }
}
void Profiler::EndManual(uint64_t timestamp)
{
    if (actuallyPaused)
        return;
    auto now = std::chrono::time_point<std::chrono::nanoseconds>(std::chrono::nanoseconds(timestamp));

    if (activeScopes.empty())
    {
        std::cerr << "Error: End called without a matching Begin" << std::endl;
        return;
    }

    auto* scope = activeScopes.top();
    activeScopes.pop();
    auto diff = now - scope->startTime;
    scope->totalTime += std::chrono::duration_cast<std::chrono::microseconds>(diff).count();
}

void Profiler::BeginFrame()
{
    BeginFrameManual(std::chrono::high_resolution_clock::now().time_since_epoch().count());
}

void Profiler::EndFrame()
{
    EndFrameManual(std::chrono::high_resolution_clock::now().time_since_epoch().count());
}

void Profiler::BeginFrameManual(uint64_t timestamp)
{
    actuallyPaused = paused;
    if (actuallyPaused)
        return;
    inProfiling = true;
    while (!activeScopes.empty())
    {
        activeScopes.pop();
    }
    BeginManual("Root", timestamp);
}
void Profiler::EndFrameManual(uint64_t timestamp)
{

    if (actuallyPaused)
        return;

    EndManual(timestamp);

    currentFrame = (currentFrame + 1) % MAX_FRAME_TRACKED;
    if (currentFrame == 0)
        trackCycles += 1;
    inProfiling = false;
};
