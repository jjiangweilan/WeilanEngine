#pragma once
#include "IProfiler.hpp"

class NoOpProfiler : public IProfiler
{
public:
    // Core profiling interface - all no-op implementations
    bool IsPaused() const override { return false; }
    void Pause() const override { /* no-op */ }
    void Resume() const override { /* no-op */ }
    void Begin(std::string_view label) override { /* no-op */ }
    void End() override { /* no-op */ }

    // Frame profiling - no-op implementations
    void BeginFrame() override { /* no-op */ }
    void EndFrame() override { /* no-op */ }

    // Manual timestamp profiling - no-op implementations
    void BeginFrameManual(uint64_t timestamp) override { /* no-op */ }
    void EndFrameManual(uint64_t timestamp) override { /* no-op */ }
    void BeginManual(std::string_view label, uint64_t timestamp) override { /* no-op */ }
    void EndManual(uint64_t timestamp) override { /* no-op */ }

    // Data retrieval - return empty/default values
    const ProfileScope& GetLatestProfile() const override
    {
        static const ProfileScope emptyScope = {};
        return emptyScope;
    }

    std::vector<float> GetFlattendFrametime() const override { return std::vector<float>(); }

    int GetLatestFrameIndex() const override { return 0; }

    const std::vector<std::unique_ptr<ProfileScope>>& GetFrameProfiles() const override
    {
        static const std::vector<std::unique_ptr<ProfileScope>> emptyProfiles;
        return emptyProfiles;
    }

    int GetFrameIndex() const override { return 0; }
    int GetTrackCycles() const override { return 0; }
};
