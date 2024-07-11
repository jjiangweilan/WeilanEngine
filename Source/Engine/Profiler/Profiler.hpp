#pragma once
#include <chrono>
#include <iostream>
#include <stack>
#include <string>
#include <vector>

constexpr int MAX_FRAME_TRACKED = 100;

struct ProfileScope
{
    std::string label;
    std::chrono::high_resolution_clock::time_point startTime;
    int64_t totalTime = 0;
    std::vector<ProfileScope> children;
};

class Profiler
{
public:
    Profiler()
    {
        frameProfiles.resize(MAX_FRAME_TRACKED);
        frameTimes.resize(MAX_FRAME_TRACKED);
    }

    void Begin(const std::string& label)
    {
        auto now = std::chrono::high_resolution_clock::now();
        ProfileScope newScope{label, now, 0, {}};

        if (!activeScopes.empty())
        {
            activeScopes.top()->children.push_back(newScope);
            activeScopes.push(&activeScopes.top()->children.back());
        }
        else
        {
            frameProfiles[currentFrame].push_back(newScope);
            activeScopes.push(&frameProfiles[currentFrame].back());
        }
    }

    void End()
    {
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
        currentFrameStart = std::chrono::high_resolution_clock::now();
        frameProfiles[currentFrame].clear();
        while (!activeScopes.empty())
        {
            activeScopes.pop();
        }
    }

    void EndFrame()
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - currentFrameStart).count();
        frameTimes[currentFrame] = duration;

        currentFrame = (currentFrame + 1) % MAX_FRAME_TRACKED;
    }

    void Report() const
    {
        std::cout << "Frame Times Report (Last " << MAX_FRAME_TRACKED << " frames):\n";
        for (int i = 0; i < MAX_FRAME_TRACKED; ++i)
        {
            std::cout << "Frame " << i << ": " << frameTimes[i] << " microseconds\n";
        }

        std::cout << "Nested Profile Report (Last frame):\n";
        printNestedProfiles(frameProfiles[(currentFrame - 1 + MAX_FRAME_TRACKED) % MAX_FRAME_TRACKED], 0);
    }

    const std::vector<std::vector<ProfileScope>>& GetFrameProfiles() const
    {
        return frameProfiles;
    }

private:
    std::stack<ProfileScope*> activeScopes;
    std::vector<std::vector<ProfileScope>> frameProfiles;
    std::vector<int64_t> frameTimes;
    int currentFrame = 0;
    std::chrono::high_resolution_clock::time_point currentFrameStart;

    void printNestedProfiles(const std::vector<ProfileScope>& scopes, int indentLevel) const
    {
        for (const auto& scope : scopes)
        {
            for (int i = 0; i < indentLevel; ++i)
                std::cout << "  ";
            std::cout << scope.label << ": " << scope.totalTime << " microseconds\n";
            printNestedProfiles(scope.children, indentLevel + 1);
        }
    }
};
