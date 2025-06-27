#if CONFIG
name: Sprite
cull: none
stencil:
    testEnable: True
    writeMask: 0xFF
    reference: 1
    passOp: replace
    compareOp: always
#endif

import WeilanEngine;
import Library.PerScene;

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

struct UniformInput
{
    float4 uvScaleOffset;
    Texture2D texture;
    SamplerState sampler_point;
};

[Global]
ParameterBlock<PerScene> perScene;

[Material]
ParameterBlock<UniformInput> uInput;

struct PushConstant
{
    float4x4 model;
    float4x4 invTspModel;
};

[[push_constant]]
PushConstant pconst;

[shader("vertex")]
VertexOutput SpriteVertex(float3 position : POSITION, float2 uv : TEXCOORD0)
{
    VertexOutput output;
    output.position = mul(perScene.camera.viewProjection, mul(pconst.model, float4(position, 1.0)));
    output.uv = uv * uInput.uvScaleOffset.xy + uInput.uvScaleOffset.zw;
    return output;
}

[shader("fragment")]
float4 SpriteFragment(VertexOutput input) : SV_Target
{
    float2 uv = input.uv;
    float4 color = uInput.texture.Sample(uInput.sampler_point, uv);

    if (color.a < 0.5)
        discard;

    return color;
}
