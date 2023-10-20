#ifndef VSM_INCLUDED
#define VSM_INCLUDED

#ifdef VSM_SHADOW_MAP_PASS
vec2 VSMShadowMap(float depth)
{
    return vec2(depth, depth * depth);
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

float VSMShadowAttenuation(vec2 moments, float fragDepth, float bleedingClamp)
{
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
