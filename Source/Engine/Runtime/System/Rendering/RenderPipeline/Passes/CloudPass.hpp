#pragma once
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Runtime/Module/VolumetricCloud/Cloud.hpp"
#include "Engine/Runtime/System/Rendering/Material.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipelinePass.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"

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
        "VolumetricCloud/VolumetricCloud";
};
} // namespace Rendering::Passes
