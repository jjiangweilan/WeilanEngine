#include "Core/Texture.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "Rendering/Shader.hpp"

namespace Libs::Image
{
void GenerateIiradianceCubemap(float* source, int width, int height, int channels, float*& output)
{
    TextureDescription desc;
    desc.data = (uint8_t*)source;
    desc.keepData = false;
    desc.img.width = width;
    desc.img.height = height;
    desc.img.isCubemap = true;
    desc.img.mipLevels = 1;
    desc.img.format = Gfx::ImageFormat::R32G32B32A32_SFloat;
    desc.img.depth = 1;
    desc.img.multiSampling = Gfx::MultiSampling::Sample_Count_1;

    Texture t(desc);

    auto cmd = GetGfxDriver()->CreateCommandBuffer();
    cmd->SetTexture("_Source", *t.GetGfxImage());

    std::unique_ptr<Gfx::Buffer> buf = GetGfxDriver()->CreateBuffer();
    ComputeShader compute;

    cmd->SetBuffer("_Params", *buf);
    cmd->BindShaderProgram(compute.GetDefaultShaderProgram(), compute.GetDefaultShaderConfig());
    cmd->Dispatch(16,16,1);
    cmd->AsyncReadback();

    GetGfxDriver()->ExecuteCommandBufferImmediately();

}
}
