import SceneLit;

export struct VertexInputSkinned : IVertexInput = SkinnedVertexInput;

#define MAX_BONE_SIZE 256
struct Bones
{
    float4x4 bones[MAX_BONE_SIZE];
};

ParameterBlock<Bones> skin;

[shader("vertex")]
VertexOutput VertexMain(VertexInputSkinned input)
{
    float4x4 boneMatrix = float4x4(0);
    for(int i = 0; i < 4; ++i)
    {
        float boneWeight = input.GetBone()[i];
        if (boneWeight != 0)
        {
            int boneIndex = int(boneWeight * 0.1);
            float weight = boneWeight - boneIndex * 10;
            boneMatrix += skin.bones[boneIndex] * weight;
        }
    }

    VertexOutput output;
    float3 vertexPos = mul(boneMatrix, float4(input.GetPosition(), 1.0)).xyz;
    float3 normal = input.GetNormal();
    float4 tangent = input.GetTangent();

    output.positionWS = mul(pconst.model, float4(vertexPos, 1)).xyz;

    matrix<float,3,3> model33 = (float3x3)pconst.invTspModel;
    output.normalWS = mul(model33, normal).xyz;
    output.tangentWS = mul(model33, tangent.xyz);
    output.bitangentWS = tangent.w * cross(output.normalWS, output.tangentWS);
    output.uv = input.GetUV();

    output.position = mul(perScene.viewProjection, float4(output.positionWS, 1));

    return output;
}

[shader("fragment")]
FragmentOutput FragmentMain(VertexOutput input)
{
    return SceneLitFragment(input);
}
