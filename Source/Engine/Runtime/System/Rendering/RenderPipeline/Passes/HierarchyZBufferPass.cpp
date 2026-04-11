#include "HierarchyZBufferPass.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Runtime/System/Rendering/RenderPipeline/RenderPipeline.hpp"
#include "Engine/Runtime/System/Rendering/RenderingData.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"

namespace Rendering::Passes
{
HierarchyZBufferPass::HierarchyZBufferPass()
{
    mip0Shader = ShaderLibrary::GetShader(Shaders::PostProcess_HierarchyZBuffer_Mip0);
    downsampleShader = ShaderLibrary::GetShader(Shaders::PostProcess_HierarchyZBuffer_Downsample);

    mip0Material.SetShader(mip0Shader);
    mip0Material.SetName("HierarchyZBuffer Mip0 Resource");
}

void HierarchyZBufferPass::Execute(
    Gfx::CommandBuffer& cmd,
    const Gfx::ImageIdentifier& srcDepth,
    const Gfx::RenderImageDescriptor& srcDepthDesc,
    const RenderingData& renderingData
)
{
    cmd.BeginLabel("HierarchyZBuffer", {0.1f, 0.4f, 0.7f, 1.0f});

    currentFrame = (currentFrame + 1) % 2;

    int width = srcDepthDesc.GetWidth();
    int height = srcDepthDesc.GetHeight();
    int mips = (int)glm::floor(glm::log2((float)glm::min(width, height))) + 1;

    Gfx::RenderImageDescriptor dstDesc(width, height, Gfx::GfxFormat::R32G32_SFloat);
    dstDesc.SetRandomWrite(true);
    dstDesc.SetMipLevels(mips);
    cmd.AllocateAttachment(hierarchyZBuffers[currentFrame], dstDesc);

    auto hiZImage = GetGfxDriver()->GetImageFromRenderGraph(hierarchyZBuffers[currentFrame]);

    // 1. Generate Mip 0
    {
        Gfx::ImageViewOption dstOption(0, 1, 0, 1, Gfx::ImageAspect::Color);

        mip0Material.SetTexture("src", GetGfxDriver()->GetImageFromRenderGraph(srcDepth));
        mip0Material.SetTexture("dst", hiZImage, dstOption);
        mip0Material.SetVector("srcTexelSize", glm::vec4(1.0f / width, 1.0f / height, width, height));
        mip0Material.SetVector("dstTexelSize", glm::vec4(1.0f / width, 1.0f / height, width, height));

        auto shaderProgram = mip0Material.GetShaderProgram();
        cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
        cmd.BindResource(mip0Material.GetSet(Gfx::DescriptorSetSemantics::Material), mip0Material.GetShaderResource());
        cmd.Dispatch((width + 7) / 8, (height + 7) / 8, 1);
    }

    // 2. Generate remaining mips
    if (downsampleMaterials.size() < (size_t)mips - 1)
    {
        int start = downsampleMaterials.size();
        downsampleMaterials.resize(mips - 1);
        for (int i = start; i < mips - 1; ++i)
        {
            downsampleMaterials[i] = std::make_unique<Material>();
            downsampleMaterials[i]->SetShader(downsampleShader);
            downsampleMaterials[i]->SetName(fmt::format("HierarchyZBuffer Downsample Resource Mip{}", i + 1));
        }
    }

    int srcW = width;
    int srcH = height;
    for (int i = 1; i < mips; ++i)
    {
        int dstW = (srcW + 1) / 2;
        int dstH = (srcH + 1) / 2;

        Gfx::ImageViewOption srcOption(i - 1, 1, 0, 1, Gfx::ImageAspect::Color);
        Gfx::ImageViewOption dstOption(i, 1, 0, 1, Gfx::ImageAspect::Color);

        auto& mat = *downsampleMaterials[i - 1];
        mat.SetTexture("src", hiZImage, srcOption);
        mat.SetTexture("dst", hiZImage, dstOption);
        mat.SetVector("srcTexelSize", glm::vec4(1.0f / srcW, 1.0f / srcH, srcW, srcH));
        mat.SetVector("dstTexelSize", glm::vec4(1.0f / dstW, 1.0f / dstH, dstW, dstH));

        auto shaderProgram = mat.GetShaderProgram();
        cmd.BindShaderProgram(shaderProgram, shaderProgram->GetDefaultShaderConfig());
        cmd.BindResource(mat.GetSet(Gfx::DescriptorSetSemantics::Material), mat.GetShaderResource());
        cmd.Dispatch((dstW + 7) / 8, (dstH + 7) / 8, 1);

        srcW = dstW;
        srcH = dstH;
    }

    cmd.EndLabel();

    if (renderingData.renderPipelineSettings)
    {
        debugView = renderingData.renderPipelineSettings->debugDraw.hierarchyZBuffer;
    }
}

bool HierarchyZBufferPass::DebugBlit(Gfx::ImageIdentifier& dst)
{
    if (debugView)
    {
        dst = hierarchyZBuffers[currentFrame];
        return true;
    }
    return false;
}

HierarchyZBufferPass::~HierarchyZBufferPass() {}

} // namespace Rendering::Passes
