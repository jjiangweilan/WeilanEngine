#include "CloudPass.hpp"
#include "Rendering/Shader.hpp"
#include "Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
CloudPass::CloudPass()
{
    volumetricCloud->SetShader(ShaderLibrary::GetShader(volumetricCloudShader));
}

void CloudPass::OnInit(RenderingData* renderingData)
{
}

void CloudPass::Execute(Cloud& cloud, Gfx::CommandBuffer& cmd, RenderingData& renderingData)
{
    volumetricCloud->CopyProperties(*cloud.volumetricCloud);
    volumetricCloud->SetTexture("cloudDensity", cloud.cloudNoise.baseShapeNoise.get());
    volumetricCloud->SetTexture("highFrequencyCloudDensity", cloud.cloudNoise.highFrequencyNoise.get());

    volumetricCloud->SetTexture("mainColor", renderingData.mainColor);
    volumetricCloud
        ->SetTexture("depthMap", renderingData.depthCopy, Gfx::ImageViewOption{0, 1, 0, 1, Gfx::ImageAspect::Depth});
    volumetricCloud->SetTexture("interleavedGradientNoise", renderingData.interleavedGradientNoise.GetNoiseTexture());
    cmd.BindShaderProgram(volumetricCloud->GetShader()->GetShaderProgram(), volumetricCloud->GetShaderConfig());
    cmd.BindResource(
        volumetricCloud->GetSet(Gfx::DescriptorSetSemantics::Material),
        volumetricCloud->GetShaderResource()
    );
    cmd.Dispatch((renderingData.gpuCamera->screenSize.x + 7) / 8, (renderingData.gpuCamera->screenSize.y + 7) / 8, 1);
}

} // namespace Rendering::Passes
