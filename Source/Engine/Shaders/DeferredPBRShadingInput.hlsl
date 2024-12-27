struct DeferredPBRShadingInput
{
    float4 shadowMapTexelSize;
    float shadowConstantBias;
    float shadowNormalBias;
    matrix<float, 4,4> mat;
};
