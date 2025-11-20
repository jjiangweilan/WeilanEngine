#pragma once
#include "GfxDriver/CommandBuffer.hpp"
#include "Modules/VolumetricCloud/Cloud.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Rendering/RenderingData.hpp"

namespace Rendering::Passes
{
class CloudPass : public RenderPipelinePass
{
public:
    CloudPass();

    void Execute(Cloud& cloud, Gfx::CommandBuffer& cmd, RenderingData& renderingData);

    void OnInit(RenderingData* renderingData) override;

private:
    std::unique_ptr<Material> volumetricCloud = std::make_unique<Material>();
    inline static const char* volumetricCloudShader =
        "Source/Engine/Modules/VolumetricCloud/Shaders/VolumetricCloud";
};
} // namespace Rendering::Passes
