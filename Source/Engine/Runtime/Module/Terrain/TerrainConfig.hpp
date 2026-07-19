#pragma once

#include "Engine/Core/Asset.hpp"
#include "Engine/Core/BinaryAsset.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Engine/Library/Math.hpp"
#include <cstdint>
#include <memory>
#include <span>
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

    std::span<const uint16_t> GetHeightSamples() const { return heightSamples; }
    std::span<const TerrainGeometryNormalSample> GetGeometryNormalSamples() const { return geometryNormalSamples; }
    void ApplyHeightValues(std::span<const TerrainHeightValue> values);
    float SampleNormalized(const float2& uv) const;
    float SampleHeight(const float2& uv) const;

    Texture* GetHeightTexture();
    Texture* GetGeometryNormalTexture();
    Mesh* GetGridMesh();
    uint64_t GetMeshRevision() const { return meshRevision; }
    uint64_t GetMaterialRevision() const { return materialRevision; }

private:
    float2 size = float2(100.0f, 100.0f);
    float2 heightRange = float2(-32.0f, 32.0f);
    uint32_t heightMapResolution = DefaultHeightMapResolution;
    uint32_t vertexResolution = DefaultVertexResolution;
    float3 baseColor = float3(0.35f, 0.45f, 0.25f);
    float roughness = 0.9f;
    float metallic = 0.0f;
    ObjPtr<BinaryAsset> heightDataAsset;

    std::vector<uint16_t> heightSamples;
    std::vector<TerrainGeometryNormalSample> geometryNormalSamples;
    std::unique_ptr<Texture> heightTexture;
    std::unique_ptr<Texture> geometryNormalTexture;
    std::unique_ptr<Mesh> gridMesh;
    uint64_t meshRevision = 1;
    uint64_t materialRevision = 1;

    void RecreateHeightTexture();
    void UploadHeightTexture();
    void RebuildGeometryNormalMap();
    void RebuildGeometryNormalRegion(uint32_t minX, uint32_t minY, uint32_t maxX, uint32_t maxY);
    void RecreateGeometryNormalTexture();
    void UploadGeometryNormalTexture();
    void InvalidateMesh();
};
