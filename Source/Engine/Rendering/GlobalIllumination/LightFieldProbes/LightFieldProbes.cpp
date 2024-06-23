#include "LightFieldProbes.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "ProbeBaker.hpp"

namespace Rendering::LFP
{
void LightFieldProbes::PlaceProbes(const glm::vec3& gridMin, const glm::vec3& gridMax, const glm::vec3& count)
{
    probes.clear();

    glm::vec3 delta = (gridMax - gridMin) / (count - glm::vec3(1));

    for (int32_t x = 0; x <= count.x; ++x)
    {
        for (int32_t y = 0; y <= count.y; ++y)
        {
            for (int32_t z = 0; z <= count.z; ++z)
            {
                glm::vec3 pos = gridMin + delta * glm::vec3(x, y, z);
                probes.emplace_back(pos);
            }
        }
    }
}
void LightFieldProbes::CaptureProbeGBuffers()
{
    Shader* gbufferBaker = (Shader*)AssetDatabase::Singleton()->LoadAsset(
        "_engine_internal/Shaders/LightFieldProbes/GBufferCuemapBaker.shad"
    );
    Shader* octahedralRemapBaker = (Shader*)AssetDatabase::Singleton()->LoadAsset(
        "_engine_internal/Shaders/LightFieldProbes/OctahedralRemapBaker.shad"
    );

    std::vector<ProbeBaker> probeBakers;
    for (Probe& probe : probes)
    {
        probeBakers.emplace_back(probe, gbufferBaker, octahedralRemapBaker);
    }

    auto cmd = GetGfxDriver()->CreateCommandBuffer();
    DrawList drawList;
    for (ProbeBaker& probeBaker : probeBakers)
    {
        probeBaker.Bake(*cmd, &drawList);
    }

    GetGfxDriver()->ExecuteCommandBuffer(*cmd);
    GetGfxDriver()->FlushPendingCommands();
}
void LightFieldProbes::DebugDrawProbes() {}
} // namespace Rendering::LFP
