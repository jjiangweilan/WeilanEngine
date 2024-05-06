#pragma once
#include "../NodeBlueprint.hpp"
#include "Asset/Shader.hpp"
#include "AssetDatabase/AssetDatabase.hpp"

namespace FrameGraph
{
class TileBasedShadingComputeNode : public Node
{
    DECLARE_FRAME_GRAPH_NODE(TileBasedShadingComputeNode)
    {
        output.tiles = AddOutputProperty("tiles", PropertyType::GfxBuffer);
        tileBasedShadingCompute = (ComputeShader*)AssetDatabase::Singleton()->LoadAsset(
            "_engine_internal/Shaders/Game/TileBasedShading/TileCompute.comp"
        );
    }

    void Compile() override {}

    void Execute(Gfx::CommandBuffer& cmd, RenderingData& renderingData) override
    {
        int xTileCount, yTileCount;
        PrepareAndPreprocess(renderingData.screenSize, xTileCount, yTileCount);

        cmd.SetBuffer(tilesShaderHandle, *tiles);
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
    ComputeShader* tileBasedShadingCompute;

    const glm::ivec2 tilePixelSize = {8, 8};
    const int maxLights = 256;
    const int totalBitMask = 32;
    const int bucketCountPerTile = glm::ceil(maxLights / totalBitMask);
    Gfx::ShaderBindingHandle tilesShaderHandle = Gfx::ShaderBindingHandle("Tiles");

    void PrepareAndPreprocess(glm::ivec2 screenSize, int& xTileCount, int& yTileCount)
    {
        xTileCount = glm::ceil(screenSize.x / 8.0f);
        yTileCount = glm::ceil(screenSize.y / 8.0f);
        int currentSize = tiles ? tiles->GetSize() : 0;
        size_t sizeNeeded = xTileCount * yTileCount * bucketCountPerTile * totalBitMask / 8;
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

}; // namespace FrameGraph
DEFINE_FRAME_GRAPH_NODE(TileBasedShadingComputeNode, "2954BAD9-5137-49D9-830C-F8A47ADDC619");

} // namespace FrameGraph
