import library.PerScene;

#if CONFIG
blend :
    - srcAlpha oneMinusSrcAlpha
#endif

struct Transform
{
    matrix<float, 4,4> model;
}

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float4 positionWS;
    float2 uv;
}

ParameterBlock<PerScene> perScene;

[vk::push_constant]
Transform transform;

[shader("vertex")]
VertexOutput VertexMain(VertexInput input)
{
    VertexOutput output;
    output.positionWS = mul(transform.model, float4(input.position, 1.0));
    output.position = mul(perScene.camera.viewProjection, output.positionWS);
    output.uv = input.uv * 100;

    return output;
}

// https://bgolus.medium.com/the-best-darn-grid-shader-yet-727f9278b9d8#1e7c
float PristineGrid(float2 uv, float inLineWidth)
{
    float2 lineWidth = clamp(float2(inLineWidth), 0, 1);
    float4 uvDDXY = float4(ddx(uv), ddy(uv));
    float2 uvDeriv = float2(length(uvDDXY.xz), length(uvDDXY.yw));
    bool2 invertLine = lineWidth > float2(0.5);
    float2 targetWidth = select(invertLine, float2(1.0) - lineWidth, lineWidth);
    float2 drawWidth = clamp(targetWidth, uvDeriv, float2(0.5));
    float2 lineAA = max(uvDeriv, 0.000001) * 1.5;
    float2 gridUV = abs(fract(uv) * 2.0 - 1.0);
    gridUV = select(invertLine, gridUV, 1.0 - gridUV);
    float2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    grid2 *= clamp(targetWidth / drawWidth, 0, 1);
    grid2 = lerp(grid2, targetWidth, clamp(uvDeriv * 2.0 - 1.0, 0, 1));
    grid2 = select(invertLine, 1.0 - grid2, grid2);
    return lerp(grid2.x, 1.0, grid2.y);
}

int GetMaxIndex(float3 val)
{
    if (val.x > val.y && val.x > val.z) return 0;
    if (val.y > val.z) return 1;
    return 2;
}

int GetMaxIndex(float2 val)
{
    if (val.x > val.y) return 0;
    return 1;
}

float2 GetFirstTwoLargestValue(float3 val, out int2 oIndex)
{
    int max0 = GetMaxIndex(val);
    int2 index = int2((max0 + 1) % 3, (max0 + 2) % 3);
    float2 order2 = float2(val[index.x], val[index.y]);
    int max1 = GetMaxIndex(order2);
    float2 result = float2(val[max0], order2[max1]);
    oIndex = int2(max0, index[max1]);
    return result;
}

float PristineGrid3D(float3 pos, float inLineWidth)
{
    float2 lineWidth = clamp(float2(inLineWidth), 0, 1);
    float3 posDDX = ddx(pos);
    float3 posDDY = ddy(pos);

    float3 posDeriv3 = float3(length(float2(posDDX.x, posDDY.x)), length(float2(posDDX.y, posDDY.y)), length(float2(posDDX.z, posDDY.z)));

    int2 index;
    float2 uvDeriv = GetFirstTwoLargestValue(posDeriv3, index);
    bool2 invertLine = lineWidth > float2(0.5);
    float2 targetWidth = select(invertLine, float2(1.0) - lineWidth, lineWidth);
    float2 drawWidth = clamp(targetWidth, uvDeriv, float2(0.5));
    float2 lineAA = max(uvDeriv, 0.000001) * 1.5;
    // float2 gridUV = abs(fract(float2(pos[index.x], pos[index.y])) * 2.0 - 1.0);
    float2 gridUV = abs(fract(float2(pos[index.x], pos[index.y])) * 2.0 - 1.0);
    gridUV = select(invertLine, gridUV, 1.0 - gridUV);
    float2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);
    grid2 *= clamp(targetWidth / drawWidth, 0, 1);
    grid2 = lerp(grid2, targetWidth, clamp(uvDeriv * 2.0 - 1.0, 0, 1));
    grid2 = select(invertLine , 1.0 - grid2, grid2);
    return lerp(grid2.x, 1.0, grid2.y);
}

[shader("fragment")]
float4 FragmentMain(VertexOutput input) : SV_Target
{
    return float4(1,1,1, PristineGrid(input.uv, 0.01f));
}


