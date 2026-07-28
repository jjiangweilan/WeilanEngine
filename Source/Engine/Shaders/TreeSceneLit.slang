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

struct TreeVertexOutput
{
    VertexOutput base;
    float billboardFade;
};

 TreeVertexOutput TreeSceneLitVertex(uint vertexIndex, uint drawIndex)
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
    // float3 worldPosition = mul(objectModelMatrix, float4(entity.GetPosition(), 1.0)).xyz;
    matrix<float, 3, 3> model33 = hasBoneWeight ? (float3x3)modelMatrix : (float3x3)invTspModelMatrix;

    TreeVertexOutput output = (TreeVertexOutput)0;
    output.base.position = mul(perScene.camera.viewProjection, float4(worldPosition, 1.0f));
    output.base.positionWS = worldPosition;
    output.base.normalWS = mul(model33, normal).xyz;
    output.base.tangentWS = mul(model33, tangent.xyz);
    output.base.bitangentWS = tangent.w * cross(output.base.normalWS, output.base.tangentWS);
    output.base.uv = entity.GetUV();
    output.base.objectOffset = drawExtra.objectOffset;
    output.base.renderDataIndex = drawExtra.renderDataIndex;

    float3 viewDirectionWS =
        normalize(worldPosition - perScene.camera.position.xyz);
    float3 trueNormal = entity.GetColor(perScene, 0).xyz * 2 - 1;
    // blender attribute export fix
    trueNormal = float3(
        trueNormal.x,
        trueNormal.z,
        -trueNormal.y
    );

    float3 trueNormalWS = normalize(mul((float3x3)invTspModelMatrix, trueNormal));
    float billboardFade = saturate(abs(dot(viewDirectionWS, trueNormalWS)));

    output.billboardFade = billboardFade;
    return output;
}

[shader("vertex")]
TreeVertexOutput VertexMain(uint vertexID : SV_VertexID, uint drawIndex : SV_DrawIndex, uint geometryOffset : SV_InstanceID)
{
    return TreeSceneLitVertex(vertexID, drawIndex);
}

[shader("fragment")]
FragmentOutput FragmentMain(TreeVertexOutput input)
{
    FragmentOutput output = SceneLitFragment(input.base, _AlphaClip);
    return output;
}
