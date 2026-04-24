import WeilanEngine;
import Library.PerScene;
import SphericalHarmonics;

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct PushConstant
{
    float4x4 model;
    float4x4 invTspModel;
} 

struct VertexOutput
{
    float4 position : SV_Position;
    float4 normalWS;
};

struct PerMaterial
{
    StructuredBuffer<float4> sh;
};

[Global]
ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<PerMaterial> perMaterial;

[vk_push_constant]
PushConstant pconst;

[shader("vertex")]
VertexOutput Vert(VertexInput input)
{
    VertexOutput output;
    output.position = mul(perScene.camera.viewProjection, mul(pconst.model, float4(input.position, 1)));
    output.normalWS = mul(pconst.model, float4(input.normal, 0));

    return output;
}

[shader("fragment")]
float4 Frag(VertexOutput input)
{
    float3 normal = normalize(input.normalWS.xyz);

    float3 coefficients[9];
    coefficients[0] = perMaterial.sh[0].xyz;
    coefficients[1] = perMaterial.sh[1].xyz;
    coefficients[2] = perMaterial.sh[2].xyz;
    coefficients[3] = perMaterial.sh[3].xyz;
    coefficients[4] = perMaterial.sh[4].xyz;
    coefficients[5] = perMaterial.sh[5].xyz;
    coefficients[6] = perMaterial.sh[6].xyz;
    coefficients[7] = perMaterial.sh[7].xyz;
    coefficients[8] = perMaterial.sh[8].xyz;

    float3 val = EvaluateSH(normal, coefficients);
    return float4(val, 1.0);
}



