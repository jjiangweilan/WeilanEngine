import Library.PerScene;

struct GpuTerrainMaterialData
{
    uint2 heightTextureIndex;
    uint2 normalTextureIndex;
    float4 heightRangeAndSize;
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
