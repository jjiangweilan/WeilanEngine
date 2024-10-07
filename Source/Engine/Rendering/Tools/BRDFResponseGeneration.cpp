#include "BRDFResponseGeneration.hpp"

#include "AssetDatabase/AssetDatabase.hpp"
#include "AssetDatabase/Exporters/KtxExporter.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "GfxDriver/Image.hpp"
#include "Rendering/Shader.hpp"
namespace Rendering
{
void GenerateBRDFResponseTexture(const char* path)
{
    Gfx::ImageDescription imgDesc{};
    imgDesc.width = 512;
    imgDesc.height = 512;
    imgDesc.depth = 1;
    imgDesc.isCubemap = false;
    imgDesc.mipLevels = 1;
    imgDesc.format = Gfx::ImageFormat::R32G32_SFloat;
    imgDesc.depth = 1;
    imgDesc.multiSampling = Gfx::MultiSampling::Sample_Count_1;

    std::unique_ptr<Gfx::Image> dst = GetGfxDriver()->CreateImage(
        imgDesc,
        Gfx::ImageUsage::TransferDst | Gfx::ImageUsage::Texture | Gfx::ImageUsage::Storage
    );

    std::unique_ptr<Gfx::CommandBuffer> cmd = GetGfxDriver()->CreateCommandBuffer();

    ComputeShader* compute =
        (ComputeShader*)AssetDatabase::Singleton()->LoadAsset("_engine_internal/Shaders/Utils/IBLBRDF.comp");
    Gfx::ShaderProgram* program = compute->GetShaderProgram({"BRDF_IBL"});

    glm::vec4 texelSize = {1.0f / imgDesc.width, 1.0f / imgDesc.height, imgDesc.width, imgDesc.height};
    cmd->SetPushConstant(program, &texelSize);
    cmd->SetTexture("_Dst", *dst);
    cmd->BindShaderProgram(program, program->GetDefaultShaderConfig());
    cmd->Dispatch(glm::ceil(imgDesc.width / 8), glm::ceil(imgDesc.height / 8), 1);

    Gfx::BufferImageCopyRegion regions[] = {
        {.bufferOffset = 0,
         .layers = {Gfx::ImageAspect::Color, 0, 0, 1},
         .offset = {0, 0, 0},
         .extend = {static_cast<uint32_t>(imgDesc.width), static_cast<uint32_t>(imgDesc.height), 1}}
    };
    auto readbackBuf = GetGfxDriver()->CreateBuffer(imgDesc.GetByteSize(), Gfx::BufferUsage::Transfer_Dst, true);
    cmd->CopyImageToBuffer(dst, readbackBuf, regions);

    GetGfxDriver()->ExecuteCommandBufferImmediately(*cmd);

    uint8_t* output = new uint8_t[imgDesc.GetByteSize()];
    memcpy(output, readbackBuf->GetCPUVisibleAddress(), imgDesc.GetByteSize());
    Exporters::KtxExporter::
        Export(path, output, imgDesc.width, imgDesc.height, 1, 2, 1, 1, false, false, imgDesc.format, false);
    delete[] output;
}
} // namespace Rendering
