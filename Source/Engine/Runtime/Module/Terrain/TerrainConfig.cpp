#include "TerrainConfig.hpp"
#include "Engine/Core/BinaryAsset.hpp"
#include "Engine/Driver/GfxDriver/GfxEnums.hpp"
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include "TerrainSystem.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <spdlog/spdlog.h>

DEFINE_ASSET(TerrainConfig, "F0CDFD91-F7DD-4E47-9135-78032B262C52", "TerrainConfig")

namespace
{
constexpr uint8_t HeightMapMagic[] = {'W', 'T', 'H', 'M'};
constexpr uint32_t HeightMapVersion = 1;
constexpr size_t HeightMapHeaderSize = 20;

void AppendUInt32(std::vector<uint8_t>& output, uint32_t value)
{
    output.push_back(static_cast<uint8_t>(value));
    output.push_back(static_cast<uint8_t>(value >> 8));
    output.push_back(static_cast<uint8_t>(value >> 16));
    output.push_back(static_cast<uint8_t>(value >> 24));
}

uint32_t ReadUInt32(const std::vector<uint8_t>& input, size_t& offset)
{
    const uint32_t value = static_cast<uint32_t>(input[offset]) |
                           (static_cast<uint32_t>(input[offset + 1]) << 8) |
                           (static_cast<uint32_t>(input[offset + 2]) << 16) |
                           (static_cast<uint32_t>(input[offset + 3]) << 24);
    offset += sizeof(uint32_t);
    return value;
}

uint16_t EncodeHeight(float normalized)
{
    return static_cast<uint16_t>(std::lround(glm::clamp(normalized, 0.0f, 1.0f) * 65535.0f));
}

uint8_t EncodeNormalComponent(float value)
{
    return static_cast<uint8_t>(std::lround((glm::clamp(value, -1.0f, 1.0f) * 0.5f + 0.5f) * 255.0f));
}
} // namespace

TerrainConfig::TerrainConfig() = default;
TerrainConfig::~TerrainConfig() = default;

void TerrainConfig::Serialize(Serializer* serializer) const
{
    Asset::Serialize(serializer);
    serializer->Serialize("size", size);
    serializer->Serialize("heightRange", heightRange);
    serializer->Serialize("heightMapResolution", heightMapResolution);
    serializer->Serialize("vertexResolution", vertexResolution);
    serializer->Serialize("baseColor", baseColor);
    serializer->Serialize("roughness", roughness);
    serializer->Serialize("metallic", metallic);
    serializer->Serialize("heightDataAsset", heightDataAsset);
}

void TerrainConfig::Deserialize(Serializer* serializer)
{
    Asset::Deserialize(serializer);
    serializer->Deserialize("size", size);
    serializer->Deserialize("heightRange", heightRange);
    serializer->Deserialize("heightMapResolution", heightMapResolution);
    serializer->Deserialize("vertexResolution", vertexResolution);
    serializer->Deserialize("baseColor", baseColor);
    serializer->Deserialize("roughness", roughness);
    serializer->Deserialize("metallic", metallic);
    serializer->Deserialize("heightDataAsset", heightDataAsset);
}

void TerrainConfig::OnLoaded()
{
    heightTexture.reset();
    geometryNormalTexture.reset();
    gridMesh.reset();
    ++meshRevision;
    ++materialRevision;
    ReadHeightData();
}

void TerrainConfig::SetSize(const float2& value)
{
    if (value.x <= 0.0f || value.y <= 0.0f || value == size)
        return;
    size = value;
    RebuildGeometryNormalMap();
    UploadGeometryNormalTexture();
    InvalidateMesh();
    ++materialRevision;
    SetDirty();
}

bool TerrainConfig::SetHeightRange(const float2& value, bool preserveWorldHeights)
{
    if (value.x >= value.y || value == heightRange)
        return false;

    if (preserveWorldHeights && HasValidHeightData())
    {
        const float oldExtent = heightRange.y - heightRange.x;
        const float newExtent = value.y - value.x;
        for (uint16_t& sample : heightSamples)
        {
            const float oldNormalized = static_cast<float>(sample) / 65535.0f;
            const float worldHeight = heightRange.x + oldNormalized * oldExtent;
            sample = EncodeHeight((worldHeight - value.x) / newExtent);
        }
    }

    heightRange = value;
    UploadHeightTexture();
    RebuildGeometryNormalMap();
    UploadGeometryNormalTexture();
    InvalidateMesh();
    ++materialRevision;
    SetDirty();
    if (heightDataAsset != nullptr)
        CommitHeightData();
    return true;
}

bool TerrainConfig::ResizeHeightMap(uint32_t resolution)
{
    if (resolution < MinHeightMapResolution || resolution > MaxHeightMapResolution ||
        resolution == heightMapResolution)
        return false;

    std::vector<uint16_t> resized(static_cast<size_t>(resolution) * resolution);
    if (HasValidHeightData())
    {
        const uint32_t oldResolution = heightMapResolution;
        for (uint32_t y = 0; y < resolution; ++y)
        {
            const float v = static_cast<float>(y) / static_cast<float>(resolution - 1);
            const float oldY = v * static_cast<float>(oldResolution - 1);
            const uint32_t y0 = static_cast<uint32_t>(oldY);
            const uint32_t y1 = std::min(y0 + 1, oldResolution - 1);
            const float fy = oldY - static_cast<float>(y0);
            for (uint32_t x = 0; x < resolution; ++x)
            {
                const float u = static_cast<float>(x) / static_cast<float>(resolution - 1);
                const float oldX = u * static_cast<float>(oldResolution - 1);
                const uint32_t x0 = static_cast<uint32_t>(oldX);
                const uint32_t x1 = std::min(x0 + 1, oldResolution - 1);
                const float fx = oldX - static_cast<float>(x0);
                const float a = glm::mix(
                    static_cast<float>(heightSamples[static_cast<size_t>(y0) * oldResolution + x0]),
                    static_cast<float>(heightSamples[static_cast<size_t>(y0) * oldResolution + x1]),
                    fx
                );
                const float b = glm::mix(
                    static_cast<float>(heightSamples[static_cast<size_t>(y1) * oldResolution + x0]),
                    static_cast<float>(heightSamples[static_cast<size_t>(y1) * oldResolution + x1]),
                    fx
                );
                resized[static_cast<size_t>(y) * resolution + x] =
                    static_cast<uint16_t>(std::lround(glm::mix(a, b, fy)));
            }
        }
    }
    else
    {
        const float zeroNormalized = (0.0f - heightRange.x) / (heightRange.y - heightRange.x);
        std::fill(resized.begin(), resized.end(), EncodeHeight(zeroNormalized));
    }

    const bool recreateHeightTexture = heightTexture != nullptr;
    const bool recreateNormalTexture = geometryNormalTexture != nullptr;
    heightMapResolution = resolution;
    heightSamples = std::move(resized);
    RebuildGeometryNormalMap();
    if (recreateHeightTexture)
        RecreateHeightTexture();
    if (recreateNormalTexture)
        RecreateGeometryNormalTexture();
    ++materialRevision;
    SetDirty();
    return CommitHeightData();
}

bool TerrainConfig::SetVertexResolution(uint32_t resolution)
{
    if (resolution < MinVertexResolution || resolution > MaxVertexResolution || resolution == vertexResolution)
        return false;
    vertexResolution = resolution;
    InvalidateMesh();
    SetDirty();
    return true;
}

void TerrainConfig::SetSurface(const float3& color, float newRoughness, float newMetallic)
{
    newRoughness = glm::clamp(newRoughness, 0.01f, 0.99f);
    newMetallic = glm::clamp(newMetallic, 0.0f, 1.0f);
    if (color == baseColor && newRoughness == roughness && newMetallic == metallic)
        return;
    baseColor = color;
    roughness = newRoughness;
    metallic = newMetallic;
    ++materialRevision;
    SetDirty();
}

void TerrainConfig::SetHeightDataAsset(BinaryAsset* asset)
{
    if (heightDataAsset.Get() == asset)
        return;
    heightDataAsset = asset;
    ReadHeightData();
    ++materialRevision;
    SetDirty();
}

bool TerrainConfig::InitializeFlatHeightMap()
{
    if (heightDataAsset == nullptr)
        return false;
    const float zeroNormalized = (0.0f - heightRange.x) / (heightRange.y - heightRange.x);
    heightSamples.assign(
        static_cast<size_t>(heightMapResolution) * heightMapResolution,
        EncodeHeight(zeroNormalized)
    );
    RebuildGeometryNormalMap();
    if (heightTexture != nullptr)
        RecreateHeightTexture();
    if (geometryNormalTexture != nullptr)
        RecreateGeometryNormalTexture();
    ++materialRevision;
    return CommitHeightData();
}

bool TerrainConfig::ReadHeightData()
{
    heightSamples.clear();
    geometryNormalSamples.clear();
    heightTexture.reset();
    geometryNormalTexture.reset();
    BinaryAsset* binary = heightDataAsset.Get();
    if (binary == nullptr)
        return false;

    const std::vector<uint8_t>& data = binary->GetData();
    auto reject = [this](const char* reason)
    {
        spdlog::warn("TerrainConfig {} height map rejected: {}", GetName(), reason);
        heightSamples.clear();
        geometryNormalSamples.clear();
        heightTexture.reset();
        geometryNormalTexture.reset();
        return false;
    };

    if (data.size() < HeightMapHeaderSize ||
        !std::equal(std::begin(HeightMapMagic), std::end(HeightMapMagic), data.begin()))
        return reject("invalid header");

    size_t offset = sizeof(HeightMapMagic);
    const uint32_t version = ReadUInt32(data, offset);
    const uint32_t width = ReadUInt32(data, offset);
    const uint32_t height = ReadUInt32(data, offset);
    const uint32_t sampleCount = ReadUInt32(data, offset);
    if (version != HeightMapVersion)
        return reject("unsupported version");
    if (width != heightMapResolution || height != heightMapResolution)
        return reject("dimensions do not match config");

    const uint64_t expectedCount = static_cast<uint64_t>(width) * height;
    if (sampleCount != expectedCount)
        return reject("invalid sample count");
    if (HeightMapHeaderSize + expectedCount * sizeof(uint16_t) != data.size())
        return reject("invalid payload size");

    heightSamples.resize(sampleCount);
    for (uint32_t i = 0; i < sampleCount; ++i)
    {
        heightSamples[i] = static_cast<uint16_t>(data[offset]) |
                           static_cast<uint16_t>(static_cast<uint16_t>(data[offset + 1]) << 8);
        offset += sizeof(uint16_t);
    }
    RebuildGeometryNormalMap();
    ++materialRevision;
    return true;
}

bool TerrainConfig::CommitHeightData()
{
    BinaryAsset* binary = heightDataAsset.Get();
    if (binary == nullptr || !HasValidHeightData())
        return false;

    std::vector<uint8_t> data;
    data.reserve(HeightMapHeaderSize + heightSamples.size() * sizeof(uint16_t));
    data.insert(data.end(), std::begin(HeightMapMagic), std::end(HeightMapMagic));
    AppendUInt32(data, HeightMapVersion);
    AppendUInt32(data, heightMapResolution);
    AppendUInt32(data, heightMapResolution);
    AppendUInt32(data, static_cast<uint32_t>(heightSamples.size()));
    for (uint16_t sample : heightSamples)
    {
        data.push_back(static_cast<uint8_t>(sample));
        data.push_back(static_cast<uint8_t>(sample >> 8));
    }
    binary->SetData(std::move(data));
    SetDirty();
    return true;
}

bool TerrainConfig::HasValidHeightData() const
{
    return heightSamples.size() == static_cast<size_t>(heightMapResolution) * heightMapResolution;
}

void TerrainConfig::ApplyHeightValues(std::span<const TerrainHeightValue> values)
{
    if (!HasValidHeightData())
        return;
    bool changed = false;
    uint32_t minX = heightMapResolution;
    uint32_t minY = heightMapResolution;
    uint32_t maxX = 0;
    uint32_t maxY = 0;
    for (const TerrainHeightValue& change : values)
    {
        if (change.index < heightSamples.size() && heightSamples[change.index] != change.value)
        {
            heightSamples[change.index] = change.value;
            const uint32_t x = change.index % heightMapResolution;
            const uint32_t y = change.index / heightMapResolution;
            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
            changed = true;
        }
    }
    if (changed)
    {
        RebuildGeometryNormalRegion(
            minX > 0 ? minX - 1 : 0,
            minY > 0 ? minY - 1 : 0,
            std::min(maxX + 1, heightMapResolution - 1),
            std::min(maxY + 1, heightMapResolution - 1)
        );
        UploadHeightTexture();
        UploadGeometryNormalTexture();
    }
}

float TerrainConfig::SampleNormalized(const float2& uv) const
{
    if (!HasValidHeightData())
        return 0.0f;
    const float x = glm::clamp(uv.x, 0.0f, 1.0f) * static_cast<float>(heightMapResolution - 1);
    const float y = glm::clamp(uv.y, 0.0f, 1.0f) * static_cast<float>(heightMapResolution - 1);
    const uint32_t x0 = static_cast<uint32_t>(x);
    const uint32_t y0 = static_cast<uint32_t>(y);
    const uint32_t x1 = std::min(x0 + 1, heightMapResolution - 1);
    const uint32_t y1 = std::min(y0 + 1, heightMapResolution - 1);
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    const float a = glm::mix(
        static_cast<float>(heightSamples[static_cast<size_t>(y0) * heightMapResolution + x0]),
        static_cast<float>(heightSamples[static_cast<size_t>(y0) * heightMapResolution + x1]),
        fx
    );
    const float b = glm::mix(
        static_cast<float>(heightSamples[static_cast<size_t>(y1) * heightMapResolution + x0]),
        static_cast<float>(heightSamples[static_cast<size_t>(y1) * heightMapResolution + x1]),
        fx
    );
    return glm::mix(a, b, fy) / 65535.0f;
}

float TerrainConfig::SampleHeight(const float2& uv) const
{
    return glm::mix(heightRange.x, heightRange.y, SampleNormalized(uv));
}

Texture* TerrainConfig::GetHeightTexture()
{
    if (heightTexture == nullptr && HasValidHeightData())
        RecreateHeightTexture();
    return heightTexture.get();
}

Texture* TerrainConfig::GetGeometryNormalTexture()
{
    if (geometryNormalTexture == nullptr && HasValidHeightData())
    {
        if (geometryNormalSamples.size() != heightSamples.size())
            RebuildGeometryNormalMap();
        RecreateGeometryNormalTexture();
    }
    return geometryNormalTexture.get();
}

Mesh* TerrainConfig::GetGridMesh()
{
    if (gridMesh == nullptr)
        gridMesh = TerrainSystem::CreateGridMesh(size, heightRange, vertexResolution);
    return gridMesh.get();
}

void TerrainConfig::RecreateHeightTexture()
{
    heightTexture.reset();
    if (!HasValidHeightData())
        return;

    TextureDescription description{};
    description.img.width = heightMapResolution;
    description.img.height = heightMapResolution;
    description.img.depth = 1;
    description.img.mipLevels = 1;
    description.img.multiSampling = Gfx::MultiSampling::Sample_Count_1;
    description.img.isCubemap = false;
    description.img.format = Gfx::GfxFormat::R16_UNorm;
    const size_t byteSize = heightSamples.size() * sizeof(uint16_t);
    description.data = new uint8_t[byteSize];
    std::memcpy(description.data, heightSamples.data(), byteSize);
    description.keepData = false;
    heightTexture = std::make_unique<Texture>(description);
    heightTexture->SetName(GetName() + " Height Map");
}

void TerrainConfig::UploadHeightTexture()
{
    if (!HasValidHeightData() || heightTexture == nullptr)
        return;
    std::span<uint8_t> bytes(
        reinterpret_cast<uint8_t*>(heightSamples.data()),
        heightSamples.size() * sizeof(uint16_t)
    );
    heightTexture->GetGfxImage()->SetData(bytes);
}

void TerrainConfig::RebuildGeometryNormalMap()
{
    if (!HasValidHeightData())
    {
        geometryNormalSamples.clear();
        return;
    }

    geometryNormalSamples.resize(heightSamples.size());
    RebuildGeometryNormalRegion(0, 0, heightMapResolution - 1, heightMapResolution - 1);
}

void TerrainConfig::RebuildGeometryNormalRegion(uint32_t minX, uint32_t minY, uint32_t maxX, uint32_t maxY)
{
    const float heightExtent = heightRange.y - heightRange.x;
    const float spacingX = size.x / static_cast<float>(heightMapResolution - 1);
    const float spacingZ = size.y / static_cast<float>(heightMapResolution - 1);
    auto sampleHeight = [this, heightExtent](uint32_t x, uint32_t y)
    {
        const float normalized = static_cast<float>(heightSamples[static_cast<size_t>(y) * heightMapResolution + x]) /
                                 65535.0f;
        return heightRange.x + normalized * heightExtent;
    };

    for (uint32_t y = minY; y <= maxY; ++y)
    {
        const uint32_t downY = y > 0 ? y - 1 : y;
        const uint32_t upY = std::min(y + 1, heightMapResolution - 1);
        for (uint32_t x = minX; x <= maxX; ++x)
        {
            const uint32_t leftX = x > 0 ? x - 1 : x;
            const uint32_t rightX = std::min(x + 1, heightMapResolution - 1);
            const float heightLeft = sampleHeight(leftX, y);
            const float heightRight = sampleHeight(rightX, y);
            const float heightDown = sampleHeight(x, downY);
            const float heightUp = sampleHeight(x, upY);
            const glm::vec3 normal = glm::normalize(glm::vec3(
                (heightLeft - heightRight) / (2.0f * spacingX),
                1.0f,
                (heightDown - heightUp) / (2.0f * spacingZ)
            ));
            geometryNormalSamples[static_cast<size_t>(y) * heightMapResolution + x] = {
                EncodeNormalComponent(normal.x),
                EncodeNormalComponent(normal.z),
            };
        }
    }
}

void TerrainConfig::RecreateGeometryNormalTexture()
{
    geometryNormalTexture.reset();
    if (!HasValidHeightData() || geometryNormalSamples.size() != heightSamples.size())
        return;

    TextureDescription description{};
    description.img.width = heightMapResolution;
    description.img.height = heightMapResolution;
    description.img.depth = 1;
    description.img.mipLevels = 1;
    description.img.multiSampling = Gfx::MultiSampling::Sample_Count_1;
    description.img.isCubemap = false;
    description.img.format = Gfx::GfxFormat::R8G8_UNorm;
    const size_t byteSize = geometryNormalSamples.size() * sizeof(TerrainGeometryNormalSample);
    description.data = new uint8_t[byteSize];
    std::memcpy(description.data, geometryNormalSamples.data(), byteSize);
    description.keepData = false;
    geometryNormalTexture = std::make_unique<Texture>(description);
    geometryNormalTexture->SetName(GetName() + " Geometry Normal Map");
}

void TerrainConfig::UploadGeometryNormalTexture()
{
    if (!HasValidHeightData() || geometryNormalSamples.size() != heightSamples.size() ||
        geometryNormalTexture == nullptr)
        return;
    std::span<uint8_t> bytes(
        reinterpret_cast<uint8_t*>(geometryNormalSamples.data()),
        geometryNormalSamples.size() * sizeof(TerrainGeometryNormalSample)
    );
    geometryNormalTexture->GetGfxImage()->SetData(bytes);
}

void TerrainConfig::InvalidateMesh()
{
    gridMesh.reset();
    ++meshRevision;
}
