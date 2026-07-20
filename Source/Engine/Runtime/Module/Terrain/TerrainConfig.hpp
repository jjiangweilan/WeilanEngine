#pragma once

#include "Engine/Core/Asset.hpp"
#include "Engine/Core/BinaryAsset.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Engine/Library/Math.hpp"
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

class Mesh;
class Texture;

struct TerrainHeightValue
{
    uint32_t index;
    uint16_t value;
};

struct TerrainGeometryNormalSample
{
    uint8_t encodedX;
    uint8_t encodedZ;
};
static_assert(sizeof(TerrainGeometryNormalSample) == 2);

struct TerrainLayerControlSample
{
    uint8_t x;
    uint8_t y;
    uint8_t z;
    uint8_t w;

    bool operator==(const TerrainLayerControlSample&) const = default;
};
static_assert(sizeof(TerrainLayerControlSample) == 4);

struct TerrainLayerControlValue
{
    uint32_t index;
    TerrainLayerControlSample ids;
    TerrainLayerControlSample weights;
};

struct [[SerClass]] TerrainLayer
{
    uint32_t id = 255;
    std::string name = "Terrain Layer";
    ObjPtr<Texture> albedoRoughnessTexture;
    ObjPtr<Texture> normalTexture;
    ObjPtr<Texture> heightTexture;
    ObjPtr<Texture> metallicTexture;
    float2 tileSize = float2(4.0f, 4.0f);
    float parallaxDepth = 0.05f;
};

class WEILAN_ENGINE_API TerrainConfig final : public Asset
{
    DECLARE_ASSET();

public:
    static constexpr uint32_t DefaultHeightMapResolution = 1024;
    static constexpr uint32_t DefaultVertexResolution = 257;
    static constexpr uint32_t MinHeightMapResolution = 128;
    static constexpr uint32_t MaxHeightMapResolution = 2048;
    static constexpr uint32_t MinVertexResolution = 17;
    static constexpr uint32_t MaxVertexResolution = 513;
    static constexpr uint32_t DefaultLayerControlResolution = 1024;
    static constexpr uint32_t MinLayerControlResolution = 128;
    static constexpr uint32_t MaxLayerControlResolution = 2048;
    static constexpr uint32_t MaxTerrainLayers = 255;
    static constexpr uint32_t InvalidTerrainLayerID = 255;

    TerrainConfig();
    ~TerrainConfig() override;

    void Serialize(Serializer* serializer) const override;
    void Deserialize(Serializer* serializer) override;
    void OnLoaded() override;

    const float2& GetSize() const { return size; }
    const float2& GetHeightRange() const { return heightRange; }
    uint32_t GetHeightMapResolution() const { return heightMapResolution; }
    uint32_t GetVertexResolution() const { return vertexResolution; }
    const float3& GetBaseColor() const { return baseColor; }
    float GetRoughness() const { return roughness; }
    float GetMetallic() const { return metallic; }

    void SetSize(const float2& value);
    bool SetHeightRange(const float2& value, bool preserveWorldHeights = true);
    bool ResizeHeightMap(uint32_t resolution);
    bool SetVertexResolution(uint32_t resolution);
    void SetSurface(const float3& color, float roughness, float metallic);

    BinaryAsset* GetHeightDataAsset() const { return heightDataAsset.Get(); }
    void SetHeightDataAsset(BinaryAsset* asset);
    bool InitializeFlatHeightMap();
    bool ReadHeightData();
    bool CommitHeightData();
    bool HasValidHeightData() const;

    BinaryAsset* GetLayerControlDataAsset() const { return layerControlDataAsset.Get(); }
    void SetLayerControlDataAsset(BinaryAsset* asset);
    uint32_t GetLayerControlResolution() const { return layerControlResolution; }
    bool InitializeLayerControlMaps();
    bool ReadLayerControlData();
    bool CommitLayerControlData();
    bool HasValidLayerControlData() const;
    bool ResizeLayerControlMaps(uint32_t resolution);

    const std::vector<TerrainLayer>& GetLayers() const { return layers; }
    uint32_t AddLayer();
    bool SetLayer(const TerrainLayer& layer);
    bool RemoveLayer(uint32_t id);
    void SetLayers(const std::vector<TerrainLayer>& value);

    std::span<const TerrainLayerControlSample> GetLayerIDSamples() const { return layerIDSamples; }
    std::span<const TerrainLayerControlSample> GetLayerWeightSamples() const { return layerWeightSamples; }
    bool SetLayerControlSamples(
        std::span<const TerrainLayerControlSample> ids,
        std::span<const TerrainLayerControlSample> weights
    );
    void ApplyLayerControlValues(std::span<const TerrainLayerControlValue> values);

    std::span<const uint16_t> GetHeightSamples() const { return heightSamples; }
    std::span<const TerrainGeometryNormalSample> GetGeometryNormalSamples() const { return geometryNormalSamples; }
    void ApplyHeightValues(std::span<const TerrainHeightValue> values);
    float SampleNormalized(const float2& uv) const;
    float SampleHeight(const float2& uv) const;

    Texture* GetHeightTexture();
    Texture* GetGeometryNormalTexture();
    Texture* GetLayerIDTexture();
    Texture* GetLayerWeightTexture();
    Mesh* GetGridMesh();
    uint64_t GetMeshRevision() const { return meshRevision; }
    uint64_t GetMaterialRevision() const { return materialRevision; }
    uint64_t GetHeightRevision() const { return heightRevision; }

private:
    float2 size = float2(100.0f, 100.0f);
    float2 heightRange = float2(-32.0f, 32.0f);
    uint32_t heightMapResolution = DefaultHeightMapResolution;
    uint32_t vertexResolution = DefaultVertexResolution;
    float3 baseColor = float3(0.35f, 0.45f, 0.25f);
    float roughness = 0.9f;
    float metallic = 0.0f;
    ObjPtr<BinaryAsset> heightDataAsset;
    uint32_t layerControlResolution = DefaultLayerControlResolution;
    ObjPtr<BinaryAsset> layerControlDataAsset;
    std::vector<TerrainLayer> layers;

    std::vector<uint16_t> heightSamples;
    std::vector<TerrainGeometryNormalSample> geometryNormalSamples;
    std::vector<TerrainLayerControlSample> layerIDSamples;
    std::vector<TerrainLayerControlSample> layerWeightSamples;
    std::unique_ptr<Texture> heightTexture;
    std::unique_ptr<Texture> geometryNormalTexture;
    std::unique_ptr<Texture> layerIDTexture;
    std::unique_ptr<Texture> layerWeightTexture;
    std::unique_ptr<Mesh> gridMesh;
    uint64_t meshRevision = 1;
    uint64_t materialRevision = 1;
    uint64_t heightRevision = 1;

    void RecreateHeightTexture();
    void UploadHeightTexture();
    void RebuildGeometryNormalMap();
    void RebuildGeometryNormalRegion(uint32_t minX, uint32_t minY, uint32_t maxX, uint32_t maxY);
    void RecreateGeometryNormalTexture();
    void UploadGeometryNormalTexture();
    void RecreateLayerControlTextures();
    void RecreateLayerIDTexture();
    void RecreateLayerWeightTexture();
    void UploadLayerIDTexture();
    void UploadLayerWeightTexture();
    void SanitizeLayers();
    void InvalidateMesh();
};
