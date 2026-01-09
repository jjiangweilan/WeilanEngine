#include "BloomPass.hpp"
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Driver/GfxDriver/GfxDriver.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include <spdlog/spdlog.h>

namespace Rendering::Passes
{

BloomPass::BloomPass()
{
    shader = ShaderLibrary::GetShader(Shaders::PostProcess_Bloom);
}

BloomPass::~BloomPass() {}

void BloomPass::Execute(
    Gfx::CommandBuffer& cmd,
    Gfx::ImageIdentifier srcColor,
    Gfx::RenderImageDescriptor srcDesc,
    const RenderPipelineSetting::PostProcess::Bloom& settings,
    const RenderingData& renderingData
)
{
    if (!settings.enabled)
        return;

    cmd.BeginLabel("Bloom", {1.0f, 0.5f, 0.5f, 1.0f});

    int width = srcDesc.GetWidth();
    int height = srcDesc.GetHeight();

    // Ensure mip chain is allocated
    int targetMip0Width = width / 2;
    int targetMip0Height = height / 2;

    if (mipChain.empty() || mipChain[0].width != targetMip0Width || mipChain[0].height != targetMip0Height)
    {
        mipChain.clear();
        int w = width;
        int h = height;
        for (int i = 0; i < kMaxMips; ++i)
        {
            w = w / 2;
            h = h / 2;
            if (w < 2 || h < 2)
                break;

            PassResource mip;
            mip.width = w;
            mip.height = h;
            mip.texture = "Bloom_Mip_" + std::to_string(i);
            mip.desc.SetWidth(w);
            mip.desc.SetHeight(h);
            mip.desc.SetFormat(Gfx::GfxFormat::R16G16B16A16_SFloat);
            mip.desc.SetRandomWrite(true); // Needed for UAV
            mipChain.push_back(mip);
        }
    }

    if (mipChain.empty())
    {
        cmd.EndLabel();
        return;
    }

    // Allocate attachments
    for (auto& mip : mipChain)
    {
        cmd.AllocateAttachment(mip.texture, mip.desc);
    }

    auto shaderProgram = shader->GetShaderProgram();
    auto& shaderConfig = shaderProgram->GetDefaultShaderConfig();

    glm::vec4 params = {settings.threshold, settings.intensity, settings.knee, settings.scatter};

    auto Dispatch = [&](int w, int h, float mode, Gfx::ImageIdentifier src, Gfx::ImageIdentifier dst, PipelineGPUBuffer& pipelineGPUBuffer, glm::vec4 texelSize)
    {
        BloomInput inputData;
        inputData.texelSize = texelSize;
        inputData.params = params;
        inputData.mode = mode;

        renderingData.pipelineAllocator->AllocateBuffer(pipelineGPUBuffer, sizeof(BloomInput));
        pipelineGPUBuffer.Write(&inputData, sizeof(BloomInput));

        cmd.BindShaderProgram(shaderProgram, shaderConfig);
        cmd.BindResource(0, {Gfx::DynamicBinding("buffer", *pipelineGPUBuffer.GetBuffer()), Gfx::DynamicBinding("src", src), Gfx::DynamicBinding("dst", dst)});
        cmd.Dispatch((w + 7) / 8, (h + 7) / 8, 1);
    };

    // 2. Prefilter (srcColor -> Mip0)
    // Mode 0
    {
        int w = mipChain[0].width;
        int h = mipChain[0].height;
        Dispatch(w, h, 0.0f, srcColor, mipChain[0].texture, mipChain[0].bloomInputBuffer, glm::vec4(1.0f / w, 1.0f / h, w, h));
    }

    // 3. Downsample Chain
    for (size_t i = 0; i < mipChain.size() - 1; ++i)
    {
        int w = mipChain[i + 1].width;
        int h = mipChain[i + 1].height;
        Dispatch(w, h, 1.0f, mipChain[i].texture, mipChain[i + 1].texture, mipChain[i + 1].bloomInputBuffer, glm::vec4(1.0f / w, 1.0f / h, w, h));
    }

    // 4. Upsample Chain
    for (int i = mipChain.size() - 2; i >= 0; --i)
    {
        int w = mipChain[i].width;
        int h = mipChain[i].height;
        // dst = Mip i (High Res), src = Mip i+1 (Low Res)
        Dispatch(w, h, 2.0f, mipChain[i + 1].texture, mipChain[i].texture, mipChain[i + 1].bloomInputBuffer, glm::vec4(1.0f / w, 1.0f / h, w, h));
    }

    // 5. Composite
    {
        int w = srcDesc.GetWidth();
        int h = srcDesc.GetHeight();
        // dst = srcColor (Main), src = Mip0
        Dispatch(w, h, 3.0f, mipChain[0].texture, srcColor, composite.bloomInputBuffer, glm::vec4(1.0f / w, 1.0f / h, w, h));
    }

    cmd.EndLabel();
}

} // namespace Rendering::Passes