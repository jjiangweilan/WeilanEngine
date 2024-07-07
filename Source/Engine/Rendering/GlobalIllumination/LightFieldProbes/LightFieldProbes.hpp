#pragma once
#include "Probe.hpp"
#include <vector>
class Scene;
namespace Rendering::LFP
{
class LightFieldProbes
{
public:
    LightFieldProbes();
    ~LightFieldProbes();
    // place probes in grid vertices
    void PlaceProbes(
        const glm::vec3& basePosition, const glm::vec3& gridMin, const glm::vec3& gridMax, const glm::vec3& count
    );

    size_t GetProbeIndex(const glm::vec3& index)
    {
        int i = index.z * probeCount.y * probeCount.x + index.y * probeCount.x + index.x;
        return i;
    }

    Probe* GetProbe(const glm::vec3& index)
    {
        int i = index.z * probeCount.y * probeCount.x + index.y * probeCount.x + index.x;
        if (i >= 0 && i < probes.size())
        {
            return &probes[i];
        }
        return nullptr;
    };

    ProbeBaker* GetProbeBaker(const glm::vec3& index)
    {
        int i = index.z * probeCount.y * probeCount.x + index.y * probeCount.x + index.x;
        if (i >= 0 && i < probeBakers.size())
        {
            return probeBakers[i].get();
        }
        return nullptr;
    };

    void BakeProbeGBuffers(Scene* scene, bool debug = false);

    std::span<Probe> GetProbes()
    {
        return probes;
    }

private:
    std::vector<Probe> probes = {};
    glm::vec3 probeCount = {};
    std::vector<std::unique_ptr<ProbeBaker>> probeBakers;
};
} // namespace Rendering::LFP
