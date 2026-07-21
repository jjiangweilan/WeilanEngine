import Library.PerScene;

static const float SKY_PI = 3.14159265358979323846;
static const uint INVALID_SKYBOX_TEXTURE_INDEX = 0xFFFFFFFFu;

float3 ProceduralSky(
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

    float3 skyCol = max(skyColor - rd.y * rd.y * 0.5, 0.001);
    skyCol = lerp(skyCol, 0.85 * skyHorizonFalloffColor, pow(1.0 - max(rd.y, 0.0), 4.0));

    // from iq's shader, https://www.shadertoy.com/view/MdX3Rr
    if (hasSun)
    {
        float sundot = clamp(dot(rd, lightDir), 0.0, 1.0);
        float3 sun = 0.25 * skySunColor * pow(sundot, 5.0);
        sun += 0.25 * skySunCoreColor * pow(sundot, 64.0);
        sun += 0.2 * skySunCoreColor * pow(sundot, 512.0);
        skyCol += sun;
    }

    skyCol = lerp(skyCol, 0.68 * skyHorizonColor, pow(1.0 - max(rd.y, 0.0), 16.0));

    return skyCol * 0.45;
}

float3 Sky(ParameterBlock<PerScene> perScene, float3 rayOrigin, float3 rayDirection)
{
    float3 rd = normalize(rayDirection);
    bool hasMainLight = perScene.scene.mainLightIndex < uint32_t(perScene.scene.lightCount);
    Light mainLight = perScene.GetMainLight();
    float skyboxIntensity = hasMainLight ? mainLight.skyboxIntensity : 1.0;

    if (perScene.scene.useHDRISkybox != 0)
    {
        uint textureIndex = perScene.scene.skyboxTextureIndex;
        if (textureIndex == INVALID_SKYBOX_TEXTURE_INDEX)
        {
            return float3(0.0);
        }

        Texture2D skyboxTexture = perScene.globalTextures[textureIndex];
        uint width, height;
        skyboxTexture.GetDimensions(width, height);

        float2 uv;
        uv.x = frac(atan2(rd.z, rd.x) / (2.0 * SKY_PI) + 0.5 + perScene.scene.skyboxRotationRadians / (2.0 * SKY_PI));
        uv.y = acos(clamp(rd.y, -1.0, 1.0)) / SKY_PI;

        float halfTexelV = 0.5 / max(float(height), 1.0);
        uv.y = clamp(uv.y, halfTexelV, 1.0 - halfTexelV);

        return skyboxTexture.SampleLevel(perScene.GetGlobalSampler(1), uv, 0.0).rgb * skyboxIntensity;
    }

    float3 lightDir = hasMainLight ? mainLight.position.xyz : float3(0.0, 1.0, 0.0);
    float3 skyColor = hasMainLight ? mainLight.skyColor.xyz : float3(0.3, 0.5, 0.85);
    float3 skyHorizonFalloffColor = hasMainLight ? mainLight.skyHorizonFalloffColor.xyz : float3(0.7, 0.75, 0.85);
    float3 skyHorizonColor = hasMainLight ? mainLight.skyHorizonColor.xyz : float3(0.418, 0.394, 0.372);
    float3 skySunColor = hasMainLight ? mainLight.skySunColor.xyz : float3(1.0, 0.7, 0.4);
    float3 skySunCoreColor = hasMainLight ? mainLight.skySunCoreColor.xyz : float3(1.0, 0.8, 0.6);

    return ProceduralSky(
        rayOrigin,
        rd,
        lightDir,
        hasMainLight,
        skyColor,
        skyHorizonFalloffColor,
        skyHorizonColor,
        skySunColor,
        skySunCoreColor
    ) * skyboxIntensity;
}
