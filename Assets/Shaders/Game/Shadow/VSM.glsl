#ifndef VSM_INCLUDED
#define VSM_INCLUDED

#ifdef VSM_SHADOW_MAP_PASS
vec2 VSMShadowMap(float depth)
{
    return vec2(depth, depth * depth);
}
#endif

#ifdef VSM_SHADOW_MAP_SAMPLING_PASS

float VSMShadowAttenuation(vec2 moments, float fragDepth)
{
    float E_x2 = moments.y;
    float Ex_2 = moments.x * moments.x;
    float variance = E_x2 - Ex_2;
    float mD = moments.x - fragDepth;
    float mD_2 = mD * mD;
    float p = variance / (variance + mD_2);
    return max( p, fragDepth <= moments.x ? 1 : 0); 
}
#endif

#endif
