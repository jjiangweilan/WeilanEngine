#include "DepthAwareBilateralUpsampler.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
DepthAwareBilateralUpsampler::DepthAwareBilateralUpsampler()
{
    shader = ShaderLibrary::GetShader(Shaders::BilateralUpScale);
    resource.SetShader(shader);
    resource.SetName("DepthAwareBilateralUpsampler Resource");
}

void DepthAwareBilateralUpsampler::Setup(
    const Gfx::ImageIdentifier& lowResColor,
    const Gfx::ImageIdentifier& lowResDepth,
    const Gfx::ImageIdentifier& highResDepth,
    const Gfx::ImageIdentifier& destination,
    const GPUInput& gpuInput
)
{
    this->lowResColor = lowResColor;
    this->lowResDepth = lowResDepth;
    this->highResDepth = highResDepth;
    this->destination = destination;

    if (this->gpuInput != gpuInput)
    {
        this->gpuInput = gpuInput;

        int2 lowResTexSize = {gpuInput.highResTexSize.x / 2, gpuInput.highResTexSize.y / 2};

        // update resource
        resource.SetVector(
            "lowResTexSize",
            float4(lowResTexSize.x, lowResTexSize.y, 1.0f / lowResTexSize.x, 1.0f / lowResTexSize.y)
        );

        resource.SetVector(
            "highResTexSize",
            float4(
                gpuInput.highResTexSize.x,
                gpuInput.highResTexSize.y,
                1.0f / gpuInput.highResTexSize.x,
                1.0f / gpuInput.highResTexSize.y
            )
        );

        resource.SetFloat("kernelScale", gpuInput.kernelSize);
        resource.SetFloat("integerCoordSigma", gpuInput.integerCoordSigma);
        resource.SetFloat("depthDiffSigma", gpuInput.depthDiffSigma);
    }
}

void DepthAwareBilateralUpsampler::Execute(Gfx::CommandBuffer& cmd)
{

    int2 highResTexSize = gpuInput.highResTexSize;

    if (highResTexSize.x == 0 || highResTexSize.y == 0)
        return;

    // Set texture resources in Execute method
    resource.SetTexture("lowColor", GetGfxDriver()->GetImageFromRenderGraph(lowResColor));
    resource.SetTexture("lowDepth", GetGfxDriver()->GetImageFromRenderGraph(lowResDepth));
    resource.SetTexture("highDepth", GetGfxDriver()->GetImageFromRenderGraph(highResDepth));
    resource.SetTexture("dst", GetGfxDriver()->GetImageFromRenderGraph(destination));

    int dispatchX = (highResTexSize.x + 7) / 8;
    int dispatchY = (highResTexSize.y + 7) / 8;

    cmd.BindResource(1, resource.GetShaderResource());
    cmd.BindShaderProgram(shader->GetShaderProgram(), shader->GetShaderProgram()->GetDefaultShaderConfig());
    cmd.Dispatch(dispatchX, dispatchY, 1);
}
} // namespace Rendering::Passes
