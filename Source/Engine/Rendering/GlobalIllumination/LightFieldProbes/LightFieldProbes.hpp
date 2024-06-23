#pragma once
#include "GfxDriver/GfxDriver.hpp"
#include "Probe.hpp"
#include <vector>
namespace Rendering::LFP
{
class LightFieldProbes
{
public:
    // place probes in grid vertices
    void PlaceProbes(const glm::vec3& gridMin, const glm::vec3& gridMax, const glm::vec3& count);
    void CaptureProbeGBuffers();
    void DebugDrawProbes();

    std::span<Probe> GetProbes()
    {
        return probes;
    }

private:
    std::vector<Probe> probes;
};
} // namespace Rendering::LFP
