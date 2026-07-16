import WeilanEngine;
import Library.PerScene;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv;
}

[shader("vertex")]
VertexOutput VertexMain(uint id : SV_VertexID)
{
    float4 c_Positions[6] =
    {
        { -1, -1, 0.5, 1 },
        { -1, 1, 0.5, 1 },
        { 1, 1, 0.5, 1 },

        { -1, -1, 0.5, 1 },
        { 1, 1, 0.5, 1 },
        { 1, -1, 0.5, 1 }
    };

    VertexOutput output;

    output.position = c_Positions[id];
    output.uv = output.position.xy * 0.5 + 0.5;
    output.position.z = 1;

    return output;
}

struct MotionVectorInput
{
    Sampler2D depthTex;
    SamplerState sampler_point_clamp;
}

ParameterBlock<PerScene> perScene;
[Material]
ParameterBlock<MotionVectorInput> perMaterial;

[shader("fragment")]
float2 FragmentMain(VertexOutput input) : SV_Target
{
    float depth = perMaterial.depthTex.Sample(input.uv).x;
    
    // NDC for current frame
    float4 currentNDC = float4(input.uv * 2.0 - 1.0, depth, 1.0);
    
    // Transform to world space using current inverse VP
    float4 worldPos = mul(perScene.camera.invNDCToWorld, currentNDC);
    worldPos /= worldPos.w;
    
    // Transform to previous clip space
    float4 previousClip = mul(perScene.camera.previousViewProjection, worldPos);
    float3 previousNDC = previousClip.xyz / previousClip.w;
    
    float2 currentUV = input.uv;
    float2 previousUV = previousNDC.xy * 0.5 + 0.5;
    
    float2 jitterDelta = perScene.camera.temporalJitter.xy - perScene.camera.temporalJitter.zw;
    float2 velocity = currentUV - previousUV - jitterDelta;
    
    return velocity;
}
