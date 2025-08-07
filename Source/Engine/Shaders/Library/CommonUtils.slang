float LinearEyeDepth(float inDepth, float4 cameraZBufferParams)
{
    return 1.0 / (cameraZBufferParams.z * inDepth + cameraZBufferParams.w);
}

float3 NormalReconstructionLow(float3 viewPos)
{
    //dFdx returns either dFdxCoarse or dFdxFine. dFdy returns either dFdyCoarse or dFdyFine.
    return normalize(cross(ddx(viewPos), ddy(viewPos)));
}

// 5 tap reconstruction
// expect clampped depth
// invNDCToX takes invNDCProject or invNDCToWorld
float3 NormalReconstructionMedium(Texture2D depthTex, SamplerState depthTexSampler, float2 uv, float2 depthTexelSize, float4 cameraZBufferParams, matrix<float,4,4> invNDCToX, out float3 centerPos)
{
    float depths[5] = {
        depthTex.SampleLevel(depthTexSampler, uv, 0).x,
        depthTex.SampleLevel(depthTexSampler, uv - float2(depthTexelSize.x, 0), 0).x,
        depthTex.SampleLevel(depthTexSampler, uv + float2(depthTexelSize.x, 0), 0).x,
        depthTex.SampleLevel(depthTexSampler, uv - float2(0, depthTexelSize.y), 0).x,
        depthTex.SampleLevel(depthTexSampler, uv + float2(0, depthTexelSize.y), 0).x
    };

    const float depthC = LinearEyeDepth(depths[0], cameraZBufferParams);
    const float depthL = LinearEyeDepth(depths[1], cameraZBufferParams);
    const float depthR = LinearEyeDepth(depths[2], cameraZBufferParams);
    int h = abs(depthC - depthL) < abs(depthC - depthR) ? 1 : 2;

    const float depthU = LinearEyeDepth(depths[3], cameraZBufferParams);
    const float depthB = LinearEyeDepth(depths[4], cameraZBufferParams);
    int v = abs(depthC - depthU) < abs(depthC - depthB) ? 3 : 4;

    float4 posC = mul(invNDCToX, float4(uv * 2 - 1, depths[0], 1.0f));
    posC.xyz /= posC.w;

    float4 posH, posV;
    posH = mul(invNDCToX, float4((uv + float2((2 * (h - 1) - 1) * depthTexelSize.x, 0)) * 2 - 1, (h == 1 ? depths[1] : depths[2]), 1.0f));
    posH.xyz /= posH.w;
    posV = mul(invNDCToX, float4((uv + float2(0, (2 * (v - 3) - 1) * depthTexelSize.y)) * 2 - 1, (v == 3 ? depths[3] : depths[4]), 1.0f));
    posV.xyz /= posV.w;

    float3 n0, n1;
    if ((h == 1 && v == 3) || (h == 2 && v == 4))
    {
        n0 = posV.xyz;
        n1 = posH.xyz;
    }
    else
    {
        n0 = posH.xyz;
        n1 = posV.xyz;
    }

    float3 normal = normalize(cross(n1 - posC.xyz, n0 - posC.xyz));

    centerPos = posC.xyz;
    return normal;
}

// From Intel's XeGTAO
float4 CalculateEdges( const float centerZ, const float leftZ, const float rightZ, const float topZ, const float bottomZ )
{
    float4 edgesLRTB = float4( leftZ, rightZ, topZ, bottomZ ) - (float)centerZ;

    float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5;
    float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5;
    float4 edgesLRTBSlopeAdjusted = edgesLRTB + float4( slopeLR, -slopeLR, slopeTB, -slopeTB );
    edgesLRTB = min( abs( edgesLRTB ), abs( edgesLRTBSlopeAdjusted ) );
    return float4(saturate( ( 1.25 - edgesLRTB / (centerZ * 0.011) ) ));
}

float3 NormalReconstructionHigh(Texture2D depthTex, SamplerState depthTexSampler, float2 uv, float2 depthTexelSize, float4 cameraZBufferParams, float4x4 invNDCToX, out float3 centerPos)
{
    float centerZ = depthTex.SampleLevel(depthTexSampler, uv, 0).x;
    float leftZ = depthTex.SampleLevel(depthTexSampler, uv - float2(depthTexelSize.x, 0), 0).x;
    float rightZ = depthTex.SampleLevel(depthTexSampler, uv + float2(depthTexelSize.x, 0), 0).x;
    float topZ = depthTex.SampleLevel(depthTexSampler, uv - float2(0, depthTexelSize.y), 0).x;
    float bottomZ = depthTex.SampleLevel(depthTexSampler, uv + float2(0, depthTexelSize.y), 0).x;

    float2 ndcCenter = uv * 2 - 1;
    float2 ndcLeft = (uv - float2(depthTexelSize.x, 0)) * 2 - 1;
    float2 ndcRight = (uv + float2(depthTexelSize.x, 0)) * 2 - 1;
    float2 ndcTop = (uv - float2(0, depthTexelSize.y)) * 2 - 1;
    float2 ndcBottom = (uv + float2(0, depthTexelSize.y)) * 2 - 1;
    float4 pixCenterPosNDC = mul(invNDCToX, float4(ndcCenter, centerZ, 1.0f));
    float4 pixLPosNDC = mul(invNDCToX, float4(ndcLeft, leftZ, 1.0f));
    float4 pixRPosNDC = mul(invNDCToX, float4(ndcRight, rightZ, 1.0f));
    float4 pixTPosNDC = mul(invNDCToX, float4(ndcTop, topZ, 1.0f));
    float4 pixBPosNDC = mul(invNDCToX, float4(ndcBottom, bottomZ, 1.0f));

    float3 pixCenterPos = pixCenterPosNDC.xyz / pixCenterPosNDC.w;
    float3 pixLPos = pixLPosNDC.xyz / pixLPosNDC.w;
    float3 pixRPos = pixRPosNDC.xyz / pixRPosNDC.w;
    float3 pixTPos = pixTPosNDC.xyz / pixTPosNDC.w;
    float3 pixBPos = pixBPosNDC.xyz / pixBPosNDC.w;
    centerPos = pixCenterPos;

    // Calculate edges
    float4 edgesLRTB = float4( pixLPos.z, pixRPos.z, pixTPos.z, pixBPos.z ) - (float)pixCenterPos.z;

    float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5;
    float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5;
    float4 edgesLRTBSlopeAdjusted = edgesLRTB + float4( slopeLR, -slopeLR, slopeTB, -slopeTB );
    edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted ));
    edgesLRTB = float4(saturate(( 1.25 - edgesLRTB / (pixCenterPos.z * 0.011))));

    float4 acceptedNormals  = saturate( float4( edgesLRTB.x*edgesLRTB.z, edgesLRTB.z*edgesLRTB.y, edgesLRTB.y*edgesLRTB.w, edgesLRTB.w*edgesLRTB.x ) + 0.01 );

    //
    pixLPos = normalize(pixLPos - pixCenterPos);
    pixRPos = normalize(pixRPos - pixCenterPos);
    pixTPos = normalize(pixTPos - pixCenterPos);
    pixBPos = normalize(pixBPos - pixCenterPos);

    float3 pixelNormal =  acceptedNormals.x * cross( pixLPos, pixTPos ) +
                        + acceptedNormals.y * cross( pixTPos, pixRPos ) +
                        + acceptedNormals.z * cross( pixRPos, pixBPos ) +
                        + acceptedNormals.w * cross( pixBPos, pixLPos );
    pixelNormal = normalize( pixelNormal );

    return pixelNormal;
}


float Gaussian_v0(float v, float sigma)
{
    return exp(-0.5 * (v * v) / (sigma * sigma)) / (sqrt(2 * 3.14159265358979323846) * sigma);
}

float4 Gaussian_v(float4 v, float sigma)
{
    return exp(-1/(sigma * sigma) * (v * v));
}


