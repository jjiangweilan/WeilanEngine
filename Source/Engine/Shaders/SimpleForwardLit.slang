import Library.PerScene;
interface IVertexInput
{
    float3 GetPosition();
    float3 GetNormal();
    float4 GetTangent();
    float2 GetUV();
    float4 GetBone();
}

struct StandardVertexInput : IVertexInput
{
    float3 position;
    float3 normal;
    float4 tangent;
    float2 uv;

    float3 GetPosition() {return position;}
    float3 GetNormal() {return normal;}
    float4 GetTangent() {return tangent;}
    float2 GetUV() { return uv;}
    float4 GetBone() { return 0;}
}

extern struct VertexInput : IVertexInput = StandardVertexInput;

struct PushConstant
{
    matrix<float,4,4> model;
} 

[vk_push_constant]
PushConstant pconst;

struct VertexOutput
{
    float4 position : SV_Position;
    float3 positionWS;
    float3 normalWS;
    float3 tangentWS;
    float3 bitangentWS;
    float2 uv;
}

ParameterBlock<PerScene> perScene;

[shader("vertex")]
VertexOutput VertexMain(VertexInput input)
{
    VertexOutput output;
    float3 vertexPos = input.GetPosition();
    float3 normal = input.GetNormal();
    float4 tangent = input.GetTangent();

    output.positionWS = mul(pconst.model, float4(vertexPos, 1)).xyz;

    matrix<float,3,3> model33 = (matrix<float,3,3>)pconst.model; // maybe a invTspModel calculated in CPU if it's needed
    output.normalWS = mul(model33, normal).xyz;
    output.tangentWS = mul(model33, tangent.xyz);
    output.bitangentWS = tangent.w * cross(output.normalWS, output.tangentWS);
    output.uv = input.GetUV();

    output.position = mul(perScene.camera.viewProjection, float4(output.positionWS, 1));

    return output;
}

[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target
{
    float3 color = float(0.6).xxx;
    if(perScene.scene.lightCount > 0)
    {
        float3 ndol = dot(input.normalWS, perScene.scene.lights[0].position.xyz);
        color *= ndol;
    }

    return float4(color, 1.0);
}
