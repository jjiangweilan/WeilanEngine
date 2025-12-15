#pragma once
#include "Driver/GfxDriver/CommandBuffer.hpp"
#include "Runtime/Module/VolumetricCloud/Cloud.hpp"
#include "Runtime/System/Rendering/Material.hpp"
#include "Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Runtime/System/Rendering/RenderingData.hpp"

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
        "Source/Engine/Module/VolumetricCloud/Shaders/VolumetricCloud";
};
} // namespace Rendering::Passes
