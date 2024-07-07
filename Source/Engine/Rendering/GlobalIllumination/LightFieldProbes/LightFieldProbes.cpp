#include "LightFieldProbes.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Scene/Scene.hpp"
#include "ProbeBaker.hpp"

namespace Rendering::LFP
{
LightFieldProbes::LightFieldProbes() {}

LightFieldProbes::~LightFieldProbes() {}

void LightFieldProbes::PlaceProbes(
    const glm::vec3& basePosition, const glm::vec3& gridMin, const glm::vec3& gridMax, const glm::vec3& count
)
{
    this->probeCount = count;
    probes.clear();

    glm::vec3 delta = (gridMax - gridMin) / glm::max(count - glm::vec3(1), glm::vec3(1, 1, 1));

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
void LightFieldProbes::BakeProbeGBuffers(Scene* scene, bool debug)
{
    probeBakers.clear();
	for (auto& probe : probes)
	{
		probeBakers.emplace_back(std::make_unique<ProbeBaker>(probe));
	}
    auto cmd = GetGfxDriver()->CreateCommandBuffer();
    DrawList drawList;
    drawList.Add(scene->GetRenderingScene().GetMeshRenderers());

    for (size_t i = 0; i < probes.size(); ++i)
    {
        probes[i].Bake(*cmd, &drawList, *probeBakers[i]);
    }

    GetGfxDriver()->ExecuteCommandBuffer(*cmd);
    GetGfxDriver()->FlushPendingCommands();

    if (!debug)
    {
        probeBakers.clear();
        probeBakers = std::vector<std::unique_ptr<ProbeBaker>>();
    }
}
} // namespace Rendering::LFP
