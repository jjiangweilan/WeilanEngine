import WeilanEngine;
import Library.PerScene;
import Library.CommonUtils;

#if CONFIG
cull: none
blend:
- one srcAlpha
depth:
    testEnable: true
    writeEnable: false
#endif

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 positionWS;
    float3 normalWS;
    float3 tangentWS;
    float3 bitangetWS;
    float2 uv;
}

struct Ps
{
    float4x4 modelMatrix;
    float4x4 invTspModelMatrix;
}

struct TransparentPass
{
    Texture2D colorCopy;
    Texture2D depthCopy;
}

struct Material
{
    [Texture] Texture2D normalMap;
    [Texture] Texture2D flowMap;

    float flowScale;

    SamplerState _Linear_Sampler;
}

[Global]
ParameterBlock<PerScene> perScene;

[Pass]
ParameterBlock<TransparentPass> pass;

[Material]
ParameterBlock<Material> mat;

[vk_push_constant]
Ps pushConstants;

[shader("vertex")]
VertexOutput Vertex(VertexInput input)
{
    float3 positionWS = mul(pushConstants.modelMatrix, float4(input.position, 1.0)).xyz;
    float3 normalWS = mul((float3x3)pushConstants.invTspModelMatrix, input.normal);
    float3 tangentWS = mul((float3x3)pushConstants.invTspModelMatrix, input.tangent.xyz);
    float4 ndc = perScene.WorldToClipSpace(positionWS);

    VertexOutput output;
    output.position = ndc;
    output.positionWS = positionWS;
    output.normalWS = normalWS;
    output.tangentWS = tangentWS;
    output.bitangetWS = cross(normalWS, tangentWS);
    output.uv = input.uv;

    return output;

}

[shader("fragment")]
float4 Fragment(VertexOutput input) : SV_Target
{
    float2 flowUV = input.uv + float2(-1,0) * mat.flowScale * perScene.scene.time;
    float3 normalMap = normalize(mat.normalMap.Sample(mat._Linear_Sampler, flowUV).xyz * 2 - 1);

    float3 normalWS = mul(transpose(float3x3(normalize(input.tangentWS), normalize(input.bitangetWS), normalize(input.normalWS))), normalMap);
    Light mainLight = perScene.GetMainLight();

    float2 screenUV = (input.position.xy) * perScene.camera.screenSize.zw;
    float ndcDepth = pass.depthCopy.Sample(mat._Linear_Sampler, screenUV + (normalMap.xy) * 0.01).x;

    float objectEyeDepth = LinearEyeDepth(input.position.z, perScene.camera.cameraZBufferParams);
    float worldEyeDepth = LinearEyeDepth(ndcDepth, perScene.camera.cameraZBufferParams);
    float eyeDepthDiff = max(worldEyeDepth - objectEyeDepth, 0);

    float ripple = eyeDepthDiff < 0.1 ? 1 : 0;

    float waterFog = saturate(exp(-eyeDepthDiff));

    float ndotl = saturate(dot(normalWS, mainLight.position.xyz));
    return float4((lerp(float3(0.1,0.3,1.) , 4, ripple)).xyz, ripple == 1 ? 1 : waterFog);
}
