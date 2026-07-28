import WeilanEngine;
import Library.PerScene;
import Lighting;
import ObjectWorldPosition;
import SceneLitFragment;

static extern const bool _AlphaClip = false;
static extern const bool _BaseColorMap = false;
static extern const bool _NormalMap = false;
static extern const bool _EmissiveMap = false;
static extern const bool _MetallicRoughnessMap = false;
static extern const bool _GPUDriven = true;

[MaterialUILayout]
struct SceneLitUI
{
    [Color, Group("Surface")]
    float4 baseColorFactor;

    [Range(0, 1), Group("Surface")]
    float roughness;

    [Range(0, 1), Group("Surface")]
    float metallic;

    [Range(0, 1), Group("Surface")]
    float alphaCutoff;

    [Range(0, 1), Group("Shadow")]
    float shadowIntensityScale;

    [Color, Group("Emission")]
    float3 emissive;

    [Texture, Group("Textures")]
    int baseColorTex;

    [Texture, Group("Textures")]
    int normalMap;

    [Texture, Group("Textures")]
    int metallicRoughnessMap;

    [Texture, Group("Textures")]
    int emissiveMap;
};

struct PushConstant
{
    uint firstGpuObjectOffset;
};

[vk_push_constant]
PushConstant pconst;

VertexOutput SceneLitVertex(uint vertexIndex, uint drawIndex)
{
    GpuDrawExtra drawExtra = perScene.gpuObjectOffsets[pconst.firstGpuObjectOffset + drawIndex];
    ObjectEntity entity = ObjectEntity(perScene, drawExtra.objectOffset, drawExtra.renderDataIndex, vertexIndex);

    float4x4 modelMatrix = entity.GetModelMatrix();
    float4x4 invTspModelMatrix = entity.GetInvModelMatrix();

    // calculation
    float3 vertexPos = entity.GetPosition();
    float3 normal = entity.GetNormal();
    float4 tangent = entity.GetTangent();

    bool hasBoneWeight = false;
    float4x4 objectModelMatrix = GetObjectModelMatrix(perScene, modelMatrix, entity.skeletonOffset, entity.GetBone(), hasBoneWeight);
    float3 worldPosition = mul(objectModelMatrix, float4(entity.GetPosition(), 1.0)).xyz;
    float4 positionNDC = mul(perScene.camera.viewProjection, float4(worldPosition, 1));
    matrix<float,3,3> model33 = hasBoneWeight ? (float3x3)modelMatrix : (float3x3)invTspModelMatrix;

    VertexOutput output = (VertexOutput)0;
    output.position = positionNDC;
    output.positionWS = worldPosition;
    output.normalWS = mul(model33, normal).xyz;
    output.tangentWS = mul(model33, tangent.xyz);
    output.bitangentWS = tangent.w * cross(output.normalWS, output.tangentWS);
    output.uv = entity.GetUV();
    output.objectOffset = drawExtra.objectOffset;
    output.renderDataIndex = drawExtra.renderDataIndex;


    return output;
}

[shader("vertex")]
VertexOutput VertexMain(uint vertexID : SV_VertexID, uint drawIndex : SV_DrawIndex, uint geometryOffset : SV_InstanceID)
{
    return SceneLitVertex(vertexID, drawIndex);
}

FragmentOutput SceneLitFragment(VertexOutput input)
{
    return SceneLitFragment(input, _AlphaClip);
}

[shader("fragment")]
FragmentOutput FragmentMain(VertexOutput input)
{
    return SceneLitFragment(input);
}
