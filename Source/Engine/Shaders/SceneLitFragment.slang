import WeilanEngine;
import Library.PerScene;
import Lighting;

struct VertexOutput
{
    float4 position : SV_Position;
    float3 positionWS;
    float3 normalWS;
    float3 tangentWS;
    float3 bitangentWS;
    float2 uv;
    nointerpolation uint objectOffset;
    nointerpolation uint renderDataIndex;
};

[Global]
ParameterBlock<PerScene> perScene;

struct FragmentOutput
{
    float4 color : SV_Target0;
    float4 albedo : SV_Target1;
    float4 normal : SV_Target2;
    float4 property : SV_Target3;
}

FragmentOutput SceneLitFragment(VertexOutput input, bool alphaClip)
{
    FragmentOutput output;

    float4 baseColor;
    float metallic;
    float roughness;
    float3 normalSample;
    float4 emission;

    GpuObject objData = perScene.LoadData<GpuObject>(input.objectOffset);
    GpuRenderData renderData = perScene.LoadData<GpuRenderData>(objData.pRenderDataOffset, input.renderDataIndex);
    GpuMaterial matData = perScene.LoadData<GpuMaterial>(renderData.materialOffset);

    baseColor = matData.baseColorFactor;
    if (matData.baseColorTexIndex.x != 0xFFFFFFFF)
    {
        baseColor *= perScene.globalTextures[matData.baseColorTexIndex.x].Sample(
            perScene.GetGlobalSampler(matData.baseColorTexIndex.y), input.uv);
    }

    if (alphaClip && baseColor.a < matData.alphaCutoff)
        discard;

    metallic = matData.metallic;
    roughness = matData.roughness;
    if (matData.metallicRoughnessTexIndex.x != 0xFFFFFFFF)
    {
        float2 metallicRoughness = float2(roughness, metallic) *
            perScene.globalTextures[matData.metallicRoughnessTexIndex.x].Sample(
                perScene.GetGlobalSampler(matData.metallicRoughnessTexIndex.y), input.uv).gb;
        metallic = metallicRoughness.y;
        roughness = clamp(metallicRoughness.x, 0.01, 0.99f);
    }
    else
        roughness = clamp(roughness, 0.01, 0.99f);

    normalSample = float3(0, 0, 1);
    if (matData.normalMapTexIndex.x != 0xFFFFFFFF)
    {
        normalSample = perScene.globalTextures[matData.normalMapTexIndex.x].Sample(
            perScene.GetGlobalSampler(matData.normalMapTexIndex.y), input.uv).xyz * 2 - 1;
    }

    emission = float4(0, 0, 0, 0);
    if (matData.emissiveMapTexIndex.x != 0xFFFFFFFF)
    {
        emission = perScene.globalTextures[matData.emissiveMapTexIndex.x].Sample(
            perScene.GetGlobalSampler(matData.emissiveMapTexIndex.y), input.uv);
    }
    emission *= matData.emissive;

    float3x3 TBN = transpose(float3x3(
        normalize(input.tangentWS),
        normalize(input.bitangentWS),
        normalize(input.normalWS)
    ));
    float3 n = EncodeGBufferNormal(normalize(mul(TBN, normalSample)));

    output.color = float4(emission.xyz, 1.0f);
    output.albedo = float4(baseColor.xyz, 1.0f);
    output.normal = float4(n, 1.0f);
    output.property = float4(metallic, roughness, 0, 1.0f);
    return output;
}
