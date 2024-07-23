#ifndef GTAO_INCLUDED
#define GTAO_INCLUDED

#include "Common/SceneInfo.glsl"
#include "Common/DeferredShading.glsl"
#include "Utils/NormalReconstruction.glsl"

layout(set = 0, binding = 3) uniform sampler2D normal_point;
layout(set = 2, binding = 0) uniform GTAO
{
    float scaling;
    float falloff;
} gtao;

vec3 GetPostionVS(vec3 ndcPos)
{
    vec4 posVS = scene.invProjection * vec4(ndcPos, 1.0);
    return posVS.xyz / posVS.w;
}

#define GTAO_SLICE_COUNT 8
#define GTAO_DIRECTION_SAMPLE_COUNT 8
#define GTAO_PI_HALF 1.5707963267f

// side: -1 or 1
float UpdateHorizon(vec2 omega, int side, vec3 posVS, vec3 viewVS)
{
    float cHorizonCos = -1;
    for (uint sampleIndex = 1; sampleIndex <= GTAO_DIRECTION_SAMPLE_COUNT; ++sampleIndex)
    {
        float s = float(sampleIndex) / float(GTAO_DIRECTION_SAMPLE_COUNT);
        vec2 scaling = scene.screenSize.zw * gtao.scaling;
        vec2 sTexCoord = uv + side * s * scaling * vec2(omega[0], -omega[1]);
        float sDepth = texture(depthTex, sTexCoord).x;
        vec3 sPosV = GetPostionVS(vec3(sTexCoord * 2 - 1, sDepth));

        if(sPosV != posVS)
        {
            float distSqr = dot(sPosV - posVS, sPosV - posVS);
            float distFalloff = clamp(distSqr * gtao.falloff, 0, 1);
            float horizonCos = mix(dot(normalize(sPosV - posVS), viewVS), -1, distFalloff);
            cHorizonCos = max(cHorizonCos, horizonCos);
        }
        else
            cHorizonCos = -2;
    }

    return cHorizonCos;
}

float GetSSAO()
{
    vec3 posVS;
    vec3 normalVS = NormalReconstructionMedium(depthTex, uv, scene.screenSize.zw, scene.cameraZBufferParams, scene.invProjection, posVS);
    vec3 viewVS = normalize(-posVS);

    float visibility = 0;

    for (int slice = 0; slice < GTAO_SLICE_COUNT; ++slice)
    {
        float phi = float(slice) * 3.1415926 / float(GTAO_SLICE_COUNT);

        vec2 omega = vec2(cos(phi), sin(phi));

        vec3 dirV = vec3(omega, 0);
        vec3 orthoDirectionV = dirV - dot(dirV, viewVS) * viewVS;
        vec3 axisV = normalize(cross(dirV, viewVS));
        vec3 projNormalV = normalVS - axisV * dot(normalVS, axisV);

        const float projNormalLength = length(projNormalV);

        float signN = sign(dot(orthoDirectionV, projNormalV));
        float cosN = clamp(dot(projNormalV, viewVS) / projNormalLength, 0.0f, 1.0f);
        float n = signN * acos(cosN);

        vec2 cHorizonCos = vec2(-1.0f, -1.0f);
        cHorizonCos.x = UpdateHorizon(omega, -1, posVS, viewVS);
        cHorizonCos.y = UpdateHorizon(omega, 1, posVS, viewVS);

        if (any(equal(cHorizonCos,vec2(-2))))
        {
            visibility += 1;
        }
        else
        {
            vec2 hSide = n + clamp(vec2(-1, 1) * acos(cHorizonCos) - n, -GTAO_PI_HALF, GTAO_PI_HALF);
            vec2 finalSide = cosN + 2 * hSide * sin(n) - cos(2 * hSide - n);
            visibility += 0.25 * projNormalLength * (finalSide.x + finalSide.y);
        }
    }

    visibility /= GTAO_SLICE_COUNT;
    return visibility;
}


#endif
