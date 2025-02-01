import Library.PerScene;

#if CONFIG
cull: none
depth:
    testEnable: True
    writeEnable: False
    compareOp: equal
#endif

// https://www.shadertoy.com/view/wslyWs
float3 Sky(float3 ro, float3 rd, float3 lightDir, bool hasSun)
{
    const float SC = 1e5;

    // Calculate sky plane
    float dist = (SC - ro.y) / rd.y; 
    float2 p = (ro + dist * rd).xz;
    p *= 1.2 / SC;

    float3 skyCol = float3(0.3,0.5,0.85) - rd.y*rd.y*0.5;
    skyCol = lerp( skyCol, 0.85 * float3(0.7,0.75,0.85), pow( 1.0 - max(rd.y, 0.0), 4.0 ) );

    // from iq's shader, https://www.shadertoy.com/view/MdX3Rr
    // sun
    if (hasSun)
    {
        float sundot = clamp(dot(rd, lightDir), 0.0, 1.0);
        float3 sun = 0.25 * float3(1.0,0.7,0.4) * pow( sundot,5.0 );
        sun += 0.25 * float3(1.0,0.8,0.6) * pow( sundot,64.0 );
        sun += 0.2 * float3(1.0,0.8,0.6) * pow( sundot,512.0 );
        skyCol += sun;
    }

    // horizon
    skyCol = lerp( skyCol, 0.68 * float3(.418, .394, .372), pow( 1.0 - max(rd.y, 0.0), 16.0 ) );

    return skyCol;
}

struct VertexInput
{
    float3 position;
}

struct VertexOutput
{
    float4 position : SV_Position;
    float3 positionOS;
}

ParameterBlock<PerScene> perScene;

[shader("vertex")]
VertexOutput Vert(VertexInput input)
{
    VertexOutput output;
    float4x4 view = perScene.view;
    view[0][3] = 0;
    view[1][3] = 0;
    view[2][3] = 0;
    view[3][3] = 1;
    float4x4 projection = perScene.projection;
    output.position = mul(projection, mul(view, float4(input.position, 1.0)));
    output.positionOS = input.position.xyz;
    output.position.z = output.position.w;

    return output;
}

[shader("fragment")]
float4 Frag(VertexOutput input) : SV_Target
{
    bool hasSun = perScene.lightCount.x > 0;
    return float4(Sky(0, normalize(input.positionOS.xyz), perScene.lights[0].position.xyz, hasSun), 1.0);
}

