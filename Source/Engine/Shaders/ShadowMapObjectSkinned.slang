import Library.PerScene;
import Skeleton;

#if CONFIG
cull: back
#endif

struct Transform
{
    matrix<float, 4,4> model;
};

[vk::push_constant]
Transform transform;

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD;
    float4 bone : BONE;
}

struct VertexOutput
{
    float4 position : SV_Position;
}

struct PerObject
{
    ConstantBuffer<Skeleton> skeleton;
};

ParameterBlock<PerScene> perScene;
ParameterBlock<PerObject> perObject;

[shader("vertex")]
VertexOutput VertexMain(VertexInput input)
{
    float4 boneWeights = input.bone;

    float4x4 skeletonMatrix = float4x4(0);
    for(int i = 0; i < 4; ++i)
    {
        float boneWeight = boneWeights[i];
        if (boneWeight != 0)
        {
            int boneIndex = int(boneWeight * 0.1);
            float weight = boneWeight - boneIndex * 10;
            skeletonMatrix += perObject.skeleton.bones[boneIndex] * weight;
        }
    }

    float3 positionWS = float3(mul(skeletonMatrix, float4(input.position, 1.0)).xyz);

    VertexOutput output;
    output.position = mul(perScene.mainLightShadow.worldToShadow, float4(positionWS, 1.0));

    return output;
}

[shader("fragment")]
void FragmentMain()
{
}
