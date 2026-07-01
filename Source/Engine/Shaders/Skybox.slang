import Library.PerScene;

#if CONFIG
cull: none
depth:
    testEnable: True
    writeEnable: False
    compOp: greaterOrEqual
#endif

// https://www.shadertoy.com/view/wslyWs
float3 Sky(
    float3 ro,
    float3 rd,
    float3 lightDir,
    bool hasSun,
    float3 skyColor,
    float3 skyHorizonFalloffColor,
    float3 skyHorizonColor,
    float3 skySunColor,
    float3 skySunCoreColor
)
{
    const float SC = 1e5;

    // Calculate sky plane
    float dist = (SC - ro.y) / (rd.y + 0.000001); 
    float2 p = (ro + dist * rd).xz;
    p *= 1.2 / SC;

    float3 skyCol = max(skyColor - rd.y*rd.y*0.5, 0.001);
    skyCol = lerp( skyCol, 0.85 * skyHorizonFalloffColor, pow( 1.0 - max(rd.y, 0.0), 4.0 ) );

    // from iq's shader, https://www.shadertoy.com/view/MdX3Rr
    // sun
    if (hasSun)
    {
        float sundot = clamp(dot(rd, lightDir), 0.0, 1.0);
        float3 sun = 0.25 * skySunColor * pow( sundot,5.0 );
        sun += 0.25 * skySunCoreColor * pow( sundot,64.0 );
        sun += 0.2 * skySunCoreColor * pow( sundot,512.0 );
        skyCol += sun;
    }

    // horizon
    skyCol = lerp( skyCol, 0.68 * skyHorizonColor, pow( 1.0 - max(rd.y, 0.0), 16.0 ) );

    return skyCol * 0.45;
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
    float4x4 view = perScene.camera.view;
    view[0][3] = 0;
    view[1][3] = 0;
    view[2][3] = 0;
    view[3][3] = 1;
    float4x4 projection = perScene.camera.projection;
    output.position = mul(projection, mul(view, float4(input.position, 1.0)));
    output.positionOS = input.position.xyz;
    output.position.z = 0;

    return output;
}

[shader("fragment")]
float4 Frag(VertexOutput input) : SV_Target
{
    bool hasSun = perScene.scene.lightCount > 0;
    float3 lightDir = float3(0.0, 1.0, 0.0);
    float3 skyColor = float3(0.3, 0.5, 0.85);
    float3 skyHorizonFalloffColor = float3(0.7, 0.75, 0.85);
    float3 skyHorizonColor = float3(0.418, 0.394, 0.372);
    float3 skySunColor = float3(1.0, 0.7, 0.4);
    float3 skySunCoreColor = float3(1.0, 0.8, 0.6);
    float lightIntensity = 1.0;

    if (hasSun)
    {
        Light light = perScene.scene.lights[0];
        lightDir = light.position.xyz;
        skyColor = light.skyColor.xyz;
        skyHorizonFalloffColor = light.skyHorizonFalloffColor.xyz;
        skyHorizonColor = light.skyHorizonColor.xyz;
        skySunColor = light.skySunColor.xyz;
        skySunCoreColor = light.skySunCoreColor.xyz;
        lightIntensity = light.intensity;
    }

    return float4(Sky(0, normalize(input.positionOS.xyz), lightDir, hasSun, skyColor, skyHorizonFalloffColor, skyHorizonColor, skySunColor, skySunCoreColor) * lightIntensity, 0.0);
}
