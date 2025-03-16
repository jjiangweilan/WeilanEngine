#include "SHProbe.hpp"
#include "Core/Scene/Scene.hpp"
#include "Rendering/RenderPipeline/RenderPipeline.hpp"

using namespace Rendering;
DEFINE_OBJECT(SHProbe, "2D474098-4932-46EA-9911-C120F5595465");

SHProbe::SHProbe() : Component(nullptr) {}
SHProbe::SHProbe(GameObject* gameObject) : Component(gameObject) {}
const std::string& SHProbe::GetName()
{
    static std::string name = "SHProbe";
    return name;
}

void SHProbe::Init(int level) {}

void SHProbe::UpdateProbe(const float4& position, const SHProbeUpdateSettings& settings)
{
    auto scene = GetScene();
    ASSERT(scene != nullptr);

    if (settings.skyboxOnly)
    {
        // render the skybox
        RenderPipeline facesPipeline[6];
        for (int face = 0; face < 6; ++face)
        {
            Camera camera;

            // 

            facesPipeline[face].RenderSkyboxOnly(*scene, *scene->GetMainCamera());
        }

        // Gfx::RG::ImageIdentifier outputColor = facesPipeline[face].GetOutputColor();
        // auto cmd = GetGfxDriver()->CreateCommandBuffer();
        //
        // GetGfxDriver()->ExecuteCommandBuffer(*cmd);
    }
    else
    {
        spdlog::warn("other sh probe not implemented");
    }
}
