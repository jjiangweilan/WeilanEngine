import Library.PerScene;

struct GpuTerrainMaterialData
{
    uint2 heightTextureIndex;
    uint2 normalTextureIndex;
    float4 heightRangeAndSize;
    uint2 layerIDTextureIndex;
    uint2 layerWeightTextureIndex;
    uint layerTableOffset;
    uint layerCount;
    uint2 padding;
};

struct GpuTerrainLayerData
{
    uint2 albedoRoughnessTextureIndex;
    uint2 normalTextureIndex;
    uint2 heightTextureIndex;
    uint2 metallicTextureIndex;
    float2 tileSize;
    float parallaxDepth;
    uint padding;
};

struct TerrainSurfaceData
{
    float3 albedo;
    float roughness;
    float metallic;
    float3 normalLS;
};

GpuTerrainMaterialData LoadTerrainMaterialData(
    ParameterBlock<PerScene> perScene,
    GpuMaterial material
)
{
    return perScene.LoadData<GpuTerrainMaterialData>(material.extraMaterialData);
}

float TerrainHeightFromSample(GpuTerrainMaterialData terrain, float normalizedHeight)
{
    return lerp(terrain.heightRangeAndSize.x, terrain.heightRangeAndSize.y, normalizedHeight);
}

float SampleTerrainHeight(
    ParameterBlock<PerScene> perScene,
    GpuTerrainMaterialData terrain,
    float2 uv
)
{
    if (terrain.heightTextureIndex.x == 0xFFFFFFFF)
        return 0.0f;

    float normalizedHeight = perScene.globalTextures[terrain.heightTextureIndex.x].SampleLevel(
        perScene.GetGlobalSampler(terrain.heightTextureIndex.y),
        clamp(uv, 0.0f, 1.0f),
        0.0f
    ).r;
    return TerrainHeightFromSample(terrain, normalizedHeight);
}

float3 ApplyTerrainDisplacement(
    ParameterBlock<PerScene> perScene,
    GpuMaterial material,
    float3 localPosition,
    float2 uv
)
{
    GpuTerrainMaterialData terrain = LoadTerrainMaterialData(perScene, material);
    localPosition.y = SampleTerrainHeight(perScene, terrain, uv);
    return localPosition;
}

float3 GetTerrainLocalNormal(ParameterBlock<PerScene> perScene, GpuMaterial material, float2 uv)
{
    GpuTerrainMaterialData terrain = LoadTerrainMaterialData(perScene, material);
    if (terrain.normalTextureIndex.x == 0xFFFFFFFF)
        return float3(0.0f, 1.0f, 0.0f);

    float2 encodedNormal = perScene.globalTextures[terrain.normalTextureIndex.x].SampleLevel(
        perScene.GetGlobalSampler(terrain.normalTextureIndex.y),
        clamp(uv, 0.0f, 1.0f),
        0.0f
    ).rg;
    float2 normalXZ = encodedNormal * 2.0f - 1.0f;
    float normalY = sqrt(saturate(1.0f - dot(normalXZ, normalXZ)));
    return normalize(float3(normalXZ.x, normalY, normalXZ.y));
}

float2 GetTerrainParallaxUV(
    ParameterBlock<PerScene> perScene,
    GpuTerrainLayerData layer,
    float2 uv,
    float2 uvDx,
    float2 uvDy,
    float3 viewDirectionTS
)
{
    const uint stepCount = 8;
    const float layerDepth = 1.0f / float(stepCount);
    float2 rayOffset = viewDirectionTS.xy / max(viewDirectionTS.z, 0.08f) *
                       (layer.parallaxDepth / layer.tileSize);
    float2 stepOffset = rayOffset / float(stepCount);
    Texture2D heightTexture = perScene.globalTextures[layer.heightTextureIndex.x];
    SamplerState heightSampler = perScene.GetGlobalSampler(layer.heightTextureIndex.y);

    float2 currentUV = uv;
    float currentDepth = 0.0f;
    float sampledDepth = 1.0f - heightTexture.SampleGrad(heightSampler, currentUV, uvDx, uvDy).r;
    [unroll]
    for (uint step = 0; step < stepCount; ++step)
    {
        if (currentDepth >= sampledDepth)
            break;
        currentUV -= stepOffset;
        currentDepth += layerDepth;
        sampledDepth = 1.0f - heightTexture.SampleGrad(heightSampler, currentUV, uvDx, uvDy).r;
    }

    float2 previousUV = currentUV + stepOffset;
    float previousDepth = currentDepth - layerDepth;
    float previousSampledDepth = 1.0f - heightTexture.SampleGrad(
        heightSampler,
        previousUV,
        uvDx,
        uvDy
    ).r;
    float afterIntersection = sampledDepth - currentDepth;
    float beforeIntersection = previousSampledDepth - previousDepth;
    float denominator = afterIntersection - beforeIntersection;
    float refinement = abs(denominator) > 0.00001f ? saturate(afterIntersection / denominator) : 0.0f;
    return lerp(currentUV, previousUV, refinement);
}

bool EvaluateTerrainLayers(
    ParameterBlock<PerScene> perScene,
    GpuMaterial material,
    GpuTerrainMaterialData terrain,
    float2 terrainUV,
    float3 localPosition,
    float3 geometryNormalLS,
    float3 viewDirectionLS,
    out TerrainSurfaceData surface
)
{
    surface.albedo = material.baseColorFactor.rgb;
    surface.roughness = material.roughness;
    surface.metallic = material.metallic;
    surface.normalLS = geometryNormalLS;
    if (terrain.layerCount == 0 || terrain.layerIDTextureIndex.x == 0xFFFFFFFF ||
        terrain.layerWeightTextureIndex.x == 0xFFFFFFFF)
        return false;

    float4 encodedIDs = perScene.globalTextures[terrain.layerIDTextureIndex.x].SampleLevel(
        perScene.GetGlobalSampler(terrain.layerIDTextureIndex.y),
        clamp(terrainUV, 0.0f, 1.0f),
        0.0f
    );
    uint4 layerIDs = uint4(round(encodedIDs * 255.0f));
    float4 layerWeights = perScene.globalTextures[terrain.layerWeightTextureIndex.x].SampleLevel(
        perScene.GetGlobalSampler(terrain.layerWeightTextureIndex.y),
        clamp(terrainUV, 0.0f, 1.0f),
        0.0f
    );

    float3 normalLS = normalize(geometryNormalLS);
    float3 tangentLS = float3(1.0f, 0.0f, 0.0f) - normalLS * normalLS.x;
    if (dot(tangentLS, tangentLS) < 0.0001f)
        tangentLS = float3(0.0f, 0.0f, -1.0f) - normalLS * -normalLS.z;
    tangentLS = normalize(tangentLS);
    float3 bitangentLS = normalize(cross(normalLS, tangentLS));
    float3 viewDirectionTS = float3(
        dot(viewDirectionLS, tangentLS),
        dot(viewDirectionLS, bitangentLS),
        dot(viewDirectionLS, normalLS)
    );

    float3 accumulatedAlbedo = 0.0f;
    float accumulatedRoughness = 0.0f;
    float accumulatedMetallic = 0.0f;
    float3 accumulatedNormalTS = 0.0f;
    float accumulatedWeight = 0.0f;
    [unroll]
    for (uint channel = 0; channel < 4; ++channel)
    {
        const uint layerID = layerIDs[channel];
        const float weight = layerWeights[channel];
        if (layerID == 255 || weight <= 0.0f)
            continue;

        const uint layerDataOffset = perScene.LoadData<uint>(
            material.extraMaterialData + terrain.layerTableOffset + layerID * sizeof(uint)
        );
        if (layerDataOffset == 0xFFFFFFFF)
            continue;

        GpuTerrainLayerData layer = perScene.LoadData<GpuTerrainLayerData>(
            material.extraMaterialData + layerDataOffset
        );
        float2 baseUV = float2(localPosition.x, -localPosition.z) / layer.tileSize;
        float2 uvDx = ddx(baseUV);
        float2 uvDy = ddy(baseUV);
        float2 layerUV = GetTerrainParallaxUV(
            perScene,
            layer,
            baseUV,
            uvDx,
            uvDy,
            viewDirectionTS
        );
        float4 albedoRoughness = perScene.globalTextures[layer.albedoRoughnessTextureIndex.x].SampleGrad(
            perScene.GetGlobalSampler(layer.albedoRoughnessTextureIndex.y),
            layerUV,
            uvDx,
            uvDy
        );
        float3 detailNormal = perScene.globalTextures[layer.normalTextureIndex.x].SampleGrad(
            perScene.GetGlobalSampler(layer.normalTextureIndex.y),
            layerUV,
            uvDx,
            uvDy
        ).rgb * 2.0f - 1.0f;
        float metallic = perScene.globalTextures[layer.metallicTextureIndex.x].SampleGrad(
            perScene.GetGlobalSampler(layer.metallicTextureIndex.y),
            layerUV,
            uvDx,
            uvDy
        ).r;

        accumulatedAlbedo += albedoRoughness.rgb * weight;
        accumulatedRoughness += albedoRoughness.a * weight;
        accumulatedMetallic += metallic * weight;
        accumulatedNormalTS += normalize(detailNormal) * weight;
        accumulatedWeight += weight;
    }

    if (accumulatedWeight <= 0.0f)
        return false;

    float inverseWeight = rcp(accumulatedWeight);
    float3 detailNormalTS = normalize(accumulatedNormalTS * inverseWeight);
    surface.albedo = accumulatedAlbedo * inverseWeight;
    surface.roughness = accumulatedRoughness * inverseWeight;
    surface.metallic = accumulatedMetallic * inverseWeight;
    surface.normalLS = normalize(
        tangentLS * detailNormalTS.x + bitangentLS * detailNormalTS.y + normalLS * detailNormalTS.z
    );
    return true;
}
