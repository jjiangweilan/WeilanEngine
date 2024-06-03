#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/Camera.hpp"

namespace Rendering::FrameGraph
{
class TileBasedShadingComputeNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(TileBasedShadingComputeNode)
    {
        output.tiles = AddOutputProperty("tiles", PropertyType::GfxBuffer);
        tileBasedShadingCompute = (ComputeShader*)AssetDatabase::Singleton()->LoadAsset(
            "_engine_internal/Shaders/Game/TileBasedShading/TileCompute.comp"
        );
        tilesCullingUbo = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
            .usages = Gfx::BufferUsage::Uniform | Gfx::BufferUsage::Transfer_Dst,
            .size = 20, // vec4 + float
            .visibleInCPU = false,
            .debugName = "tile culling ubo",
            .gpuWrite = false
        });
    }

    void Compile() override {}

    void Execute(Gfx::CommandBuffer& cmd, FrameData& renderingData) override
    {
        int xTileCount, yTileCount;
        PrepareAndPreprocess(renderingData.screenSize, xTileCount, yTileCount);

        cmd.SetBuffer(tilesShaderHandle, *tiles);
        cmd.SetBuffer(tilesUboShaderHandle, *tilesCullingUbo);
        float pushConstants[] = {
            (2 * renderingData.mainCamera->GetProjectionRight()) / (float)xTileCount,
            (2 * renderingData.mainCamera->GetProjectionTop()) / (float)yTileCount,
            (float)xTileCount,
            (float)yTileCount,
            (float)bucketCountPerTile
        };
        GetGfxDriver()->UploadBuffer(*tilesCullingUbo, (uint8_t*)&pushConstants, sizeof(pushConstants));

        cmd.BindShaderProgram(
            tileBasedShadingCompute->GetDefaultShaderProgram(),
            tileBasedShadingCompute->GetDefaultShaderConfig()
        );
        cmd.Dispatch(glm::ceil(xTileCount / 8.0f), glm::ceil(yTileCount / 8.0f), 1);

        output.tiles->SetValue(tiles.get());
    }

private:
    struct
    {
        PropertyHandle tiles;
    } output;

    std::unique_ptr<Gfx::Buffer> tiles = nullptr;
    std::unique_ptr<Gfx::Buffer> tilesCullingUbo = nullptr;
    ComputeShader* tileBasedShadingCompute;

    const glm::ivec2 tilePixelSize = {16, 16};
    const int maxLights = 128;
    const int totalBitMask = 32;
    const int bucketCountPerTile = glm::ceil(maxLights / totalBitMask);
    Gfx::ShaderBindingHandle tilesShaderHandle = Gfx::ShaderBindingHandle("Tiles");
    Gfx::ShaderBindingHandle tilesUboShaderHandle = Gfx::ShaderBindingHandle("TileComputeParams");
    Gfx::ShaderBindingHandle lightPositionHandle = Gfx::ShaderBindingHandle("LightPositions");

    void PrepareAndPreprocess(glm::ivec2 screenSize, int& xTileCount, int& yTileCount)
    {
        xTileCount = glm::ceil(screenSize.x / (float)tilePixelSize.x);
        yTileCount = glm::ceil(screenSize.y / (float)tilePixelSize.y);
        int currentSize = tiles ? tiles->GetSize() : 0;
        size_t sizeNeeded = xTileCount * yTileCount * bucketCountPerTile * sizeof(uint32_t);
        if (sizeNeeded > currentSize)
        {
            tiles = GetGfxDriver()->CreateBuffer(Gfx::Buffer::CreateInfo{
                .usages = Gfx::BufferUsage::Storage,
                .size = sizeNeeded,
                .visibleInCPU = false,
                .debugName = "light tiles",
                .gpuWrite = true

            });
        }
    }

}; // namespace Rendering::FrameGraph
DEFINE_FRAME_GRAPH_NODE(TileBasedShadingComputeNode, "2954BAD9-5137-49D9-830C-F8A47ADDC619");

} // namespace Rendering::FrameGraph
