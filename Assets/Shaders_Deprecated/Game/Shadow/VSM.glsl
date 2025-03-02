#ifndef VSM_INCLUDED
#define VSM_INCLUDED
// include this in fragment shader only because dFdx only supports in fragment shader

#ifdef VSM_SHADOW_MAP_PASS
vec2 VSMShadowMap(float depth)
{
    float dx = dFdx(depth); 
    float dy = dFdy(depth);
    return vec2(depth, depth * depth + 0.25f * (dx * dx + dy * dy));
}
#endif

#ifdef VSM_SHADOW_MAP_SAMPLING_PASS

float linstep(float min, float max, float v)
{
    return clamp((v - min) / (max - min), 0, 1);
}

float ReduceLightBleeding(float p_max, float Amount)
{
    // Remove the [0, Amount] tail and linearly rescale (Amount, 1].
    return linstep(Amount, 1, p_max);
}

float VSMShadowAttenuation(vec2 shadowUV, float fragDepth, float bleedingClamp, float mDClamp)
{
    vec2 moments = texture(sampler2D(shadowMap, shadowMapSampler), shadowUV).xy;
    float E_x2 = moments.y;
    float Ex_2 = moments.x * moments.x;
    float variance = E_x2 - Ex_2;
    float mD = fragDepth - moments.x;
    float mD_2 = mD * mD;
    float p = variance / (variance + mD_2);
    return ReduceLightBleeding(max(p, fragDepth <= moments.x ? 1 : 0), bleedingClamp); 
}
#endif

#endif
