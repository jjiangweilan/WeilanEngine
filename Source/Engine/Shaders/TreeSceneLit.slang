import WeilanEngine;
import Library.PerScene;
import SceneLitFragment;
import ObjectWorldPosition;
import TreeWind;

static extern const bool _AlphaClip = false;
static extern const bool _BaseColorMap = false;
static extern const bool _NormalMap = false;
static extern const bool _EmissiveMap = false;
static extern const bool _MetallicRoughnessMap = false;
static extern const bool _GPUDriven = true;

[MaterialUILayout]
struct TreeSceneLitUI
{
    [Color, Group("Surface")]
    float4 baseColorFactor;

    [Range(0, 1), Group("Surface")]
    float roughness;

    [Range(0, 1), Group("Surface")]
    float metallic;

    [Range(0, 1), Group("Surface")]
    float alphaCutoff;

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

    [Group("Wind")]
    float2 windDirection;

    [Range(0, 2), Group("Wind")]
    float windStrength;

    [Range(0, 10), Group("Wind")]
    float windSpeed;

    [Range(0, 2), Group("Wind")]
    float windFrequency;

    [Group("Wind")]
    float windBaseHeight;

    [Range(0.01, 100), Group("Wind")]
    float windBendHeight;
};

struct PushConstant
{
    uint firstGpuObjectOffset;
};

[vk_push_constant]
PushConstant pconst;

VertexOutput TreeSceneLitVertex(uint vertexIndex, uint drawIndex)
{
    GpuDrawExtra drawExtra = perScene.gpuObjectOffsets[pconst.firstGpuObjectOffset + drawIndex];
    ObjectEntity entity = ObjectEntity(perScene, drawExtra.objectOffset, drawExtra.renderDataIndex, vertexIndex);

    float4x4 modelMatrix = entity.GetModelMatrix();
    float4x4 invTspModelMatrix = entity.GetInvModelMatrix();
    float3 normal = entity.GetNormal();
    float4 tangent = entity.GetTangent();

    bool hasBoneWeight = false;
    float4x4 objectModelMatrix = GetObjectModelMatrix(
        perScene,
        modelMatrix,
        entity.skeletonOffset,
        entity.GetBone(),
        hasBoneWeight
    );
    GpuMaterial material = entity.GetMaterial(perScene);
    float3 worldPosition = ApplyTreeWind(
        entity.GetPosition(),
        objectModelMatrix,
        material,
        perScene.scene.time
    );
    matrix<float, 3, 3> model33 = hasBoneWeight ? (float3x3)modelMatrix : (float3x3)invTspModelMatrix;

    VertexOutput output = (VertexOutput)0;
    output.position = mul(perScene.camera.viewProjection, float4(worldPosition, 1.0f));
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
    return TreeSceneLitVertex(vertexID, drawIndex);
}

[shader("fragment")]
FragmentOutput FragmentMain(VertexOutput input)
{
    return SceneLitFragment(input, _AlphaClip);
}
