#include "LightFieldProbes.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Scene/Scene.hpp"
#include "ProbeBaker.hpp"

namespace Rendering::LFP
{
void LightFieldProbes::PlaceProbes(const glm::vec3& basePosition, const glm::vec3& gridMin, const glm::vec3& gridMax, const glm::vec3& count)
{
    probes.clear();

    glm::vec3 delta = (gridMax - gridMin) / glm::max(count - glm::vec3(1), glm::vec3(1,1,1));

    for (int32_t x = 0; x < count.x; ++x)
    {
        for (int32_t y = 0; y < count.y; ++y)
        {
            for (int32_t z = 0; z < count.z; ++z)
            {
                glm::vec3 pos = basePosition + gridMin + delta * glm::vec3(x, y, z);
                probes.emplace_back(pos);
            }
        }
    }
}
void LightFieldProbes::BakeProbeGBuffers(Scene* scene)
{
    Shader* gbufferBaker = (Shader*)AssetDatabase::Singleton()->LoadAsset(
        "_engine_internal/Shaders/LightFieldProbes/GBufferCuemapBaker.shad"
    );

    std::vector<std::unique_ptr<ProbeBaker>> probeBakers;
    for (Probe& probe : probes)
    {
        probeBakers.push_back(std::make_unique<ProbeBaker>(probe, gbufferBaker));
    }

    auto cmd = GetGfxDriver()->CreateCommandBuffer();
    DrawList drawList;
    drawList.Add(scene->GetRenderingScene().GetMeshRenderers());
    for (auto& probeBaker : probeBakers)
    {
        probeBaker->Bake(*cmd, &drawList);
    }

    GetGfxDriver()->ExecuteCommandBuffer(*cmd);
    GetGfxDriver()->FlushPendingCommands();
}
} // namespace Rendering::LFP
