#ifndef GTAO_INCLUDED
#define GTAO_INCLUDED

#include "Common/SceneInfo.glsl"
#include "Common/DeferredShading.glsl"
#include "Utils/NormalReconstruction.glsl"

#define GTAO_SLICE_COUNT 8
#define GTAO_DIRECTION_SAMPLE_COUNT 8
#define GTAO_PI 3.1415926f
#define GTAO_PI_HALF 1.5707963267f
#define GTAO_PI_2 6.283185307f
#define GTAO_VISIBILITY_MASK_SECTOR_COUNT 32

layout(set = 0, binding = 3) uniform sampler2D normal_point;
layout(set = 2, binding = 0) uniform GTAO
{
    vec2 debugPoint;
    float strength;
    float scaling;
    float falloff;
    float bias;
} gtao;

vec3 GetPostionVS(vec3 ndcPos)
{
    vec4 posVS = scene.invProjection * vec4(ndcPos, 1.0);
    return posVS.xyz / posVS.w;
}

float GTAOFastSqrt( float x )
{
    // [Drobot2014a] Low Level Optimizations for GCN
    return intBitsToFloat( 0x1FBD1DF5 + ( floatBitsToInt( x ) >> 1 ) );
}

float GTAOFastAcos( float x )
{
    // [Eberly2014] GPGPU Programming for Games and Science
    float res = -0.156583 * abs( x ) + GTAO_PI / 2.0;
    res *= GTAOFastSqrt( 1.0 - abs( x ) );
    return x >= 0 ? res : GTAO_PI - res;
}

uint UpdateSectorsVM(float minHorizon, float maxHorizon, uint globalOccludedBitfield)
{
    uint startHorizonInt = uint(minHorizon * GTAO_VISIBILITY_MASK_SECTOR_COUNT);
    uint angleHorizonInt = uint(ceil((maxHorizon-minHorizon) * GTAO_VISIBILITY_MASK_SECTOR_COUNT));
    uint angleHorizonBitfield = angleHorizonInt > 0 ? uint(0xFFFFFFFFu >> (GTAO_VISIBILITY_MASK_SECTOR_COUNT - angleHorizonInt)) : 0;
    uint currentOccludedBitfield = angleHorizonBitfield << startHorizonInt;
    return globalOccludedBitfield | currentOccludedBitfield;
}

void UpdateHorizonVM(vec2 omega, int side, vec3 posVS, vec3 viewVS, float N, inout uint globalOccludedBitfield)
{
    for (uint sampleIndex = 1; sampleIndex <= GTAO_DIRECTION_SAMPLE_COUNT; ++sampleIndex)
    {
        float s = float(sampleIndex) / float(GTAO_DIRECTION_SAMPLE_COUNT);
        vec2 scaling = scene.screenSize.zw * gtao.scaling;
        vec2 offset = side * s * scaling * vec2(omega[0], -omega[1]) / posVS.z;
        offset = sign(offset) * max(scene.screenSize.zw, abs(offset));
        vec2 sTexCoord = uv + offset;

        float sDepth = texture(depthTex, sTexCoord).x;
        vec3 sPosV = GetPostionVS(vec3(sTexCoord * 2 - 1, sDepth));

        vec3 deltaPos = sPosV - posVS;
        if(any(notEqual(deltaPos, vec3(0))))
        {
            vec3 deltaPosBackface = deltaPos - viewVS * gtao.falloff;
            // Project sample onto the unit circle and compute the angle relative to V
            vec2 frontBackHorizon = vec2(dot(normalize(deltaPos), viewVS), dot(normalize(deltaPosBackface), viewVS));
            frontBackHorizon = acos(frontBackHorizon);

            float samplingDirection = side;
            // Shift sample from V to normal, clamp in [0..1]
            frontBackHorizon = ((samplingDirection * (frontBackHorizon - gtao.bias)) - N + GTAO_PI_HALF) / GTAO_PI;
            frontBackHorizon = clamp(frontBackHorizon, 0, 1);

            // Sampling direction inverts min/max angles
            // frontBackHorizon = samplingDirection >= 0 ? frontBackHorizon.yx : frontBackHorizon.xy;
            vec2 temp = frontBackHorizon;
            frontBackHorizon.x = min(temp.x, temp.y);
            frontBackHorizon.y = max(temp.x, temp.y);

            globalOccludedBitfield = UpdateSectorsVM(frontBackHorizon.x, frontBackHorizon.y, globalOccludedBitfield);
        }
    }
}

float GetSSAOVM()
{
    vec3 posVS;
    vec3 normalVS = NormalReconstructionMedium(depthTex, uv, scene.screenSize.zw, scene.cameraZBufferParams, scene.invProjection, posVS);
    vec3 viewVS = normalize(-posVS);

    float visibility = 0;

    for (int slice = 0; slice < GTAO_SLICE_COUNT; ++slice)
    {
        float phi = GTAO_PI * float(slice) / float(GTAO_SLICE_COUNT);

        vec2 omega = vec2(cos(phi), sin(phi));

        vec3 dirV = vec3(omega, 0);
        vec3 orthoDirectionV = dirV - dot(dirV, viewVS) * viewVS;
        vec3 axisV = normalize(cross(dirV, viewVS));
        vec3 projNormalV = normalVS - axisV * dot(normalVS, axisV);

        const float projNormalLength = length(projNormalV);

        float signN = sign(dot(orthoDirectionV, projNormalV));
        float cosN = clamp(dot(projNormalV, viewVS) / projNormalLength , 0.0f, 1.0f);
        float n = signN * acos(cosN);

        uint globalOccludedBitfield = 0;
        UpdateHorizonVM(omega, -1, posVS, viewVS, n, globalOccludedBitfield);
        UpdateHorizonVM(omega, 1, posVS, viewVS, n, globalOccludedBitfield);
        visibility += 1.0 - float(bitCount(globalOccludedBitfield)) / float(GTAO_VISIBILITY_MASK_SECTOR_COUNT);
    }

    visibility /= GTAO_SLICE_COUNT;
    return clamp(visibility * gtao.strength, 0, 1);
}


// side: -1 or 1
float UpdateHorizon(vec2 omega, int side, vec3 posVS, vec3 viewVS)
{
    float cHorizonCos = -1;
    for (uint sampleIndex = 1; sampleIndex <= GTAO_DIRECTION_SAMPLE_COUNT; ++sampleIndex)
    {
        float s = float(sampleIndex) / float(GTAO_DIRECTION_SAMPLE_COUNT);
        vec2 scaling = scene.screenSize.zw * gtao.scaling;
        vec2 sTexCoord = uv + side * s * scaling * vec2(omega[0], -omega[1]) / posVS.z;
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
        float phi = GTAO_PI * float(slice) / float(GTAO_SLICE_COUNT);

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
    return clamp(visibility * gtao.strength, 0, 1);
}

#endif
