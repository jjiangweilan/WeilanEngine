#include "LightFieldProbes.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Scene/Scene.hpp"
#include "ProbeBaker.hpp"

namespace Rendering::LFP
{
void LightFieldProbes::PlaceProbes(
    const glm::vec3& basePosition, const glm::vec3& gridMin, const glm::vec3& gridMax, const glm::vec3& count
)
{
    probes.clear();

    glm::vec3 delta = (gridMax - gridMin) / glm::max(count - glm::vec3(1), glm::vec3(1));

    for (int32_t z = 0; z < count.z; ++z)
    {
        for (int32_t y = 0; y < count.y; ++y)
        {
            for (int32_t x = 0; x < count.x; ++x)
            {
                glm::vec3 pos = basePosition + gridMin + delta * glm::vec3(x, y, z);
                probes.emplace_back(pos);
            }
        }
    }
}
void LightFieldProbes::BakeProbeGBuffers(Scene* scene)
{
    auto cmd = GetGfxDriver()->CreateCommandBuffer();
    DrawList drawList;
    drawList.Add(scene->GetRenderingScene().GetMeshRenderers());

    for (auto& probe : probes)
    {
        probe.Bake(*cmd, &drawList);
    }

    GetGfxDriver()->ExecuteCommandBuffer(*cmd);
    GetGfxDriver()->FlushPendingCommands();
}
} // namespace Rendering::LFP
