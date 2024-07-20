#ifndef GTAO_INCLUDED
#define GTAO_INCLUDED

#include "Common/SceneInfo.glsl"
#include "Common/DeferredShading.glsl"

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

#define GTAO_SLICE_COUNT 16
#define GTAO_DIRECTION_SAMPLE_COUNT 16
#define GTAO_PI_HALF 1.5707963267f

// float CalculateHorizon(int side, vec3 posVS, vec3 viewVS, vec2 omega)
// {
//     float cHorizonCos = -1.0f;
//     for (int sampleIndex = 0; sampleIndex < GTAO_DIRECTION_SAMPLE_COUNT; ++sampleIndex)
//     {
//         float s = float(sampleIndex) / float(GTAO_DIRECTION_SAMPLE_COUNT);
//         vec2 scaling = scene.screenSize.zw * gtao.scaling;
//         vec2 sTexCoord = uv + side * s * scaling * vec2(omega[0], -omega[1]);
//         float sDepth = texture(depth_clamp, sTexCoord).x;
//         vec3 sPosV = GetPostionVS(vec3(sTexCoord * 2 - 1, sDepth));
//         vec3 sHorizonV = normalize(sPosV - posVS);
//         cHorizonCos = max(cHorizonCos, dot(sHorizonV, viewVS));
//     }
//
//     return cHorizonCos
// }
//
// float CalculateSide(int side, float horizon)
// {
//     float hSide = n + clamp(side * acos(horizon) - n, -GTAO_PI_HALF, GTAO_PI_HALF);
//     return cosN + 2 * hSide * sin(n) - cos(2 * hSide - n);
// }

float GetSSAO()
{
    float depthVal = texture(depth_clamp, uv).x;
    vec3 posVS = GetPostionVS(vec3(uv * 2 - 1, depthVal));
    vec3 normalVS = NormalReconstruction(posVS);
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
        for (int side = 0; side <= 1; ++side)
        {
            for (int sampleIndex = 0; sampleIndex < GTAO_DIRECTION_SAMPLE_COUNT; ++sampleIndex)
            {
                float s = float(sampleIndex) / float(GTAO_DIRECTION_SAMPLE_COUNT);
                vec2 scaling = scene.screenSize.zw * gtao.scaling;
                vec2 sTexCoord = uv + (2 * side - 1) * s * scaling * vec2(omega[0], -omega[1]);
                float sDepth = texture(depth_clamp, sTexCoord).x;
                vec3 sPosV = GetPostionVS(vec3(sTexCoord * 2 - 1, sDepth));

                if(sPosV != posVS)
                {
                    float distSqr = dot(sPosV - posVS, sPosV - posVS);
                    float distFalloff = clamp(distSqr * gtao.falloff, 0, 1);
                    float horizonCos = mix(dot(normalize(sPosV - posVS), viewVS), -1, distFalloff);
                    cHorizonCos[side] =  max(cHorizonCos[side], horizonCos);
                }
                else
                    cHorizonCos[side] = -2;
            }

        }

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
