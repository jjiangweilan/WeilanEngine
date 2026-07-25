#include "Terrain.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/Runtime/Module/Terrain/TerrainConfig.hpp"
#include "Engine/Runtime/Module/Terrain/TerrainSystem.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/Rendering/ShaderLibrary.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
constexpr uint32_t PointClampSamplerIndex = 4;
constexpr uint32_t AnisotropicRepeatSamplerIndex = 10;
constexpr uint32_t TerrainLayerTableEntryCount = 256;

uint32_t GetTextureIndex(Texture* texture)
{
    if (texture == nullptr || texture->GetGPUTextureHandle() == Rendering::InvalidGPUHandle)
        return Rendering::InvalidTextureIndex;
    return static_cast<uint32_t>(texture->GetGPUTextureHandle());
}

bool IsLayerTextureAvailable(Texture* texture)
{
    return GetTextureIndex(texture) != Rendering::InvalidTextureIndex;
}

Texture* ResolveLayerTexture(Texture* texture)
{
    return texture != nullptr ? texture : &EngineInternalResources::GetBlackTexture();
}
} // namespace

DEFINE_OBJECT(MeshRenderer, Terrain, "F9F80900-D99F-45F4-A41D-B3414B3CCB0F");

Terrain::Terrain()
    : MeshRenderer(nullptr) {}
Terrain::Terrain(GameObject* owner)
    : MeshRenderer(owner) {}

Terrain::~Terrain()
{
    SetMaterial(nullptr);
    SetMesh(nullptr);
    terrainMaterial.reset();
}

void Terrain::Serialize(Serializer* serializer) const
{
    Component::Serialize(serializer);
    serializer->Serialize("terrainConfig", terrainConfig);
}

void Terrain::Deserialize(Serializer* serializer)
{
    Component::Deserialize(serializer);
    serializer->Deserialize("terrainConfig", terrainConfig);
}

const std::string& Terrain::GetName() const
{
    static const std::string name = "Terrain";
    return name;
}

std::unique_ptr<Component> Terrain::Clone(GameObject& owner)
{
    auto clone = std::make_unique<Terrain>(&owner);
    clone->terrainConfig = terrainConfig;
    clone->enabled = enabled;
    return clone;
}

void Terrain::OnAwake()
{
    SetGPUObject(true);
    EnableRayTracing(false);
    RefreshResources(true);
}

void Terrain::OnLoaded()
{
    MeshRenderer::OnLoaded();
    RefreshResources(true);
}

void Terrain::OnDestroy()
{
    SetMaterial(nullptr);
    SetMesh(nullptr);
    terrainMaterial.reset();
}

void Terrain::Tick()
{
    RefreshResources(false);
}

void Terrain::IdleTick()
{
    RefreshResources(false);
}

void Terrain::SetTerrainConfig(TerrainConfig* config)
{
    if (terrainConfig.Get() == config)
        return;
    terrainConfig = config;
    observedMeshRevision = 0;
    observedMaterialRevision = 0;
    RefreshResources(true);
}

void Terrain::RefreshResources(bool force)
{
    TerrainConfig* config = terrainConfig.Get();
    if (config == nullptr || !config->HasValidHeightData() || config->GetHeightTexture() == nullptr ||
        config->GetGeometryNormalTexture() == nullptr)
    {
        if (force || GetMesh() != nullptr)
        {
            SetMaterial(nullptr);
            SetMesh(nullptr);
            terrainMaterial.reset();
        }
        return;
    }

    if (force || observedMeshRevision != config->GetMeshRevision())
    {
        SetMesh(config->GetGridMesh());
        observedMeshRevision = config->GetMeshRevision();
    }

    if (force || observedMaterialRevision != config->GetMaterialRevision())
    {
        RefreshMaterial();
        observedMaterialRevision = config->GetMaterialRevision();
    }
}

void Terrain::RefreshMaterial()
{
    TerrainConfig* config = terrainConfig.Get();
    if (config == nullptr)
        return;

    if (terrainMaterial == nullptr)
    {
        terrainMaterial = std::make_unique<Material>();
        terrainMaterial->SetName("Terrain Runtime Material");
        terrainMaterial->SetShader(Shaders::Terrain);
    }

    terrainMaterial->SetVector("baseColorFactor", glm::vec4(config->GetBaseColor(), 1.0f));
    terrainMaterial->SetVector("emissive", glm::vec4(0.0f));
    terrainMaterial->SetFloat("roughness", config->GetRoughness());
    terrainMaterial->SetFloat("metallic", config->GetMetallic());
    terrainMaterial->SetFloat("alphaCutoff", 0.0f);
    const float2 range = config->GetHeightRange();
    const float2 size = config->GetSize();
    Texture* heightTexture = config->GetHeightTexture();
    Texture* normalTexture = config->GetGeometryNormalTexture();
    Texture* layerIDTexture = config->GetLayerIDTexture();
    Texture* layerWeightTexture = config->GetLayerWeightTexture();

    std::vector<std::pair<uint32_t, GpuTerrainLayerData>> packedLayers;
    packedLayers.reserve(config->GetLayers().size());
    for (const TerrainLayer& layer : config->GetLayers())
    {
        Texture* albedoRoughnessTexture = ResolveLayerTexture(layer.albedoRoughnessTexture.Get());
        Texture* layerNormalTexture = ResolveLayerTexture(layer.normalTexture.Get());
        Texture* layerHeightTexture = ResolveLayerTexture(layer.heightTexture.Get());
        Texture* metallicTexture = ResolveLayerTexture(layer.metallicTexture.Get());
        if (layer.id >= TerrainConfig::MaxTerrainLayers || layer.tileSize.x <= 0.0f || layer.tileSize.y <= 0.0f ||
            !IsLayerTextureAvailable(albedoRoughnessTexture) ||
            !IsLayerTextureAvailable(layerNormalTexture) ||
            !IsLayerTextureAvailable(layerHeightTexture) ||
            !IsLayerTextureAvailable(metallicTexture))
            continue;

        packedLayers.emplace_back(layer.id, GpuTerrainLayerData{
            .albedoRoughnessTextureIndex = glm::uvec2(
                GetTextureIndex(albedoRoughnessTexture),
                AnisotropicRepeatSamplerIndex
            ),
            .normalTextureIndex = glm::uvec2(
                GetTextureIndex(layerNormalTexture),
                AnisotropicRepeatSamplerIndex
            ),
            .heightTextureIndex = glm::uvec2(
                GetTextureIndex(layerHeightTexture),
                AnisotropicRepeatSamplerIndex
            ),
            .metallicTextureIndex = glm::uvec2(
                GetTextureIndex(metallicTexture),
                AnisotropicRepeatSamplerIndex
            ),
            .tileSize = glm::max(layer.tileSize, glm::vec2(0.001f)),
            .parallaxDepth = glm::max(layer.parallaxDepth, 0.0f),
            .padding = 0,
        });
    }

    constexpr uint32_t layerTableOffset = sizeof(GpuTerrainMaterialData);
    constexpr uint32_t layerDataOffset = layerTableOffset + TerrainLayerTableEntryCount * sizeof(uint32_t);
    std::vector<uint8_t> extraData(
        layerDataOffset + packedLayers.size() * sizeof(GpuTerrainLayerData),
        0
    );
    auto* layerTable = reinterpret_cast<uint32_t*>(extraData.data() + layerTableOffset);
    std::fill_n(layerTable, TerrainLayerTableEntryCount, Rendering::InvalidTextureIndex);
    for (size_t index = 0; index < packedLayers.size(); ++index)
    {
        const uint32_t dataOffset = layerDataOffset + static_cast<uint32_t>(index * sizeof(GpuTerrainLayerData));
        layerTable[packedLayers[index].first] = dataOffset;
        std::memcpy(extraData.data() + dataOffset, &packedLayers[index].second, sizeof(GpuTerrainLayerData));
    }

    const GpuTerrainMaterialData terrainData{
        .heightTextureIndex = glm::uvec2(GetTextureIndex(heightTexture), 5),
        .normalTextureIndex = glm::uvec2(GetTextureIndex(normalTexture), 5),
        .heightRangeAndSize = glm::vec4(range.x, range.y, size.x, size.y),
        .layerIDTextureIndex = glm::uvec2(GetTextureIndex(layerIDTexture), PointClampSamplerIndex),
        .layerWeightTextureIndex = glm::uvec2(GetTextureIndex(layerWeightTexture), PointClampSamplerIndex),
        .layerTableOffset = layerTableOffset,
        .layerCount = static_cast<uint32_t>(packedLayers.size()),
        .padding = glm::uvec2(0),
    };
    std::memcpy(extraData.data(), &terrainData, sizeof(terrainData));
    terrainMaterial->SetGPUDrivenExtraData(extraData);
    SetMaterial(terrainMaterial.get());
}

bool Terrain::Raycast(
    const Ray& worldRay,
    float& outDistance,
    float3& outPoint,
    float3& outNormal
) const
{
    TerrainConfig* config = terrainConfig.Get();
    GameObject* owner = const_cast<Terrain*>(this)->GetGameObject();
    if (config == nullptr || owner == nullptr || !config->HasValidHeightData())
        return false;

    const glm::mat4 model = owner->GetWorldMatrix();
    const glm::mat4 inverseModel = glm::inverse(model);
    const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
    const glm::vec3 localOrigin = inverseModel * glm::vec4(worldRay.origin, 1.0f);
    const glm::vec3 localDirection = inverseModel * glm::vec4(worldRay.direction, 0.0f);
    const float2 size = config->GetSize();
    const float2 heightRange = config->GetHeightRange();

    const glm::vec3 boundsMin(-size.x * 0.5f, heightRange.x, -size.y * 0.5f);
    const glm::vec3 boundsMax(size.x * 0.5f, heightRange.y, size.y * 0.5f);
    float enter = 0.0f;
    float exit = std::numeric_limits<float>::max();
    for (int axis = 0; axis < 3; ++axis)
    {
        if (glm::abs(localDirection[axis]) < 1e-7f)
        {
            if (localOrigin[axis] < boundsMin[axis] || localOrigin[axis] > boundsMax[axis])
                return false;
            continue;
        }
        float a = (boundsMin[axis] - localOrigin[axis]) / localDirection[axis];
        float b = (boundsMax[axis] - localOrigin[axis]) / localDirection[axis];
        if (a > b)
            std::swap(a, b);
        enter = glm::max(enter, a);
        exit = glm::min(exit, b);
        if (enter > exit)
            return false;
    }

    const uint32_t resolution = config->GetVertexResolution();
    const float cellX = size.x / static_cast<float>(resolution - 1);
    const float cellZ = size.y / static_cast<float>(resolution - 1);
    const glm::vec3 start = localOrigin + localDirection * enter;
    int x = glm::clamp(static_cast<int>((start.x - boundsMin.x) / cellX), 0, static_cast<int>(resolution) - 2);
    int z = glm::clamp(static_cast<int>((start.z - boundsMin.z) / cellZ), 0, static_cast<int>(resolution) - 2);
    const int stepX = localDirection.x > 1e-7f ? 1 : (localDirection.x < -1e-7f ? -1 : 0);
    const int stepZ = localDirection.z > 1e-7f ? 1 : (localDirection.z < -1e-7f ? -1 : 0);
    const float infinity = std::numeric_limits<float>::infinity();
    const float nextX = boundsMin.x + static_cast<float>(stepX > 0 ? x + 1 : x) * cellX;
    const float nextZ = boundsMin.z + static_cast<float>(stepZ > 0 ? z + 1 : z) * cellZ;
    float crossX = stepX == 0 ? infinity : (nextX - localOrigin.x) / localDirection.x;
    float crossZ = stepZ == 0 ? infinity : (nextZ - localOrigin.z) / localDirection.z;
    const float deltaX = stepX == 0 ? infinity : glm::abs(cellX / localDirection.x);
    const float deltaZ = stepZ == 0 ? infinity : glm::abs(cellZ / localDirection.z);

    float closest = std::numeric_limits<float>::max();
    glm::vec3 closestNormal(0.0f, 1.0f, 0.0f);
    float cellEnter = enter;
    while (x >= 0 && z >= 0 && x < static_cast<int>(resolution) - 1 && z < static_cast<int>(resolution) - 1 &&
           cellEnter <= exit)
    {
        const float u0 = static_cast<float>(x) / static_cast<float>(resolution - 1);
        const float u1 = static_cast<float>(x + 1) / static_cast<float>(resolution - 1);
        const float v0 = static_cast<float>(z) / static_cast<float>(resolution - 1);
        const float v1 = static_cast<float>(z + 1) / static_cast<float>(resolution - 1);
        const glm::vec3 p0(boundsMin.x + x * cellX, config->SampleHeight(float2(u0, v0)), boundsMin.z + z * cellZ);
        const glm::vec3 p1(p0.x + cellX, config->SampleHeight(float2(u1, v0)), p0.z);
        const glm::vec3 p2(p0.x, config->SampleHeight(float2(u0, v1)), p0.z + cellZ);
        const glm::vec3 p3(p0.x + cellX, config->SampleHeight(float2(u1, v1)), p0.z + cellZ);
        const float cellExit = glm::min(glm::min(crossX, crossZ), exit);

        auto testTriangle = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
        {
            glm::vec2 barycentric;
            float distance = -1.0f;
            if (glm::intersectRayTriangle(localOrigin, localDirection, a, b, c, barycentric, distance) &&
                distance >= cellEnter - 1e-5f && distance <= cellExit + 1e-5f && distance < closest)
            {
                closest = distance;
                closestNormal = glm::normalize(glm::cross(b - a, c - a));
            }
        };
        testTriangle(p0, p2, p3);
        testTriangle(p0, p3, p1);
        if (closest < std::numeric_limits<float>::max())
            break;

        if (crossX < crossZ)
        {
            cellEnter = crossX;
            crossX += deltaX;
            x += stepX;
        }
        else if (crossZ < crossX)
        {
            cellEnter = crossZ;
            crossZ += deltaZ;
            z += stepZ;
        }
        else
        {
            if (stepX == 0 && stepZ == 0)
                break;
            cellEnter = crossX;
            crossX += deltaX;
            crossZ += deltaZ;
            x += stepX;
            z += stepZ;
        }
    }

    if (closest == std::numeric_limits<float>::max())
        return false;
    outDistance = closest;
    outPoint = worldRay.origin + worldRay.direction * closest;
    outNormal = glm::normalize(normalMatrix * closestNormal);
    return true;
}
