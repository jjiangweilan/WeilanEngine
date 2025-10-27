import WeilanEngine;
import Math;

struct ShaderInput
{
    float envMapSize;
    float roughness[6];

    TextureCube srcCubemap; // mipped source cubemap
    RWTexture2D dstFaces[30]; // 6 mips * 5 faces, {mips{faces...}, ...}
};

[Material]
ParameterBlock<ShaderInput> shaderInput;

// credits: https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
float3 ImportanceSampleGGX(float2 vXi, float fRoughness, float3 vNormal)
{
    // Compute the local half vector
    float fA = fRoughness * fRoughness;
    float fPhi = 2.0f * M_PI * vXi.x;
    float fCosTheta = sqrt((1.0f - vXi.y) / (1.0f + (fA*fA - 1.0f) * vXi.y));
    float fSinTheta = sqrt(1.0f - fCosTheta * fCosTheta);
    float3 vHalf;
    vHalf.x = fSinTheta * cos(fPhi);
    vHalf.y = fSinTheta * sin(fPhi);
    vHalf.z = fCosTheta;

    // Compute a tangent frame and rotate the half vector to world space
    float3 vUp = abs(vNormal.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 vTangentX = normalize(cross(vUp, vNormal));
    float3 vTangentY = cross(vNormal, vTangentX);
    // Tangent to world space
    return vTangentX * vHalf.x + vTangentY * vHalf.y + vNormal * vHalf.z;
}

[shader("compute")]
[numthreads(64,1,1)]
void main() {
    float fTotalWeight = 0.0f;
    const uint iNumSamples = 32;
    float envMapSize = shaderInput.envMapSize;

    float roughness = shaderInput.envMapSize;
    for (uint i = 0; i < iNumSamples; i++)
    {
        float2 vXi = Hammersley(i, iNumSamples);

        float3 vHalf = ImportanceSampleGGX(vXi, roughness, vNormal);
        float3 vLight = 2.0f * dot(vView, vHalf) * vHalf - vView;

        float fNdotL = saturate(dot(vNormal, vLight));
        if (fNdotL > 0.0f)
        {
            // Vectors to evaluate pdf
            float fNdotH = saturate(dot(vNormal, vHalf));
            float fVdotH = saturate(dot(vView, vHalf));

            // Probability Distribution Function
            float fPdf = D_GGX(fNdotH) * fNdotH / (4.0f * fVdotH);

            // Solid angle represented by this sample
            float fOmegaS = 1.0 / (iNumSamples * fPdf);

            // Solid angle covered by 1 pixel with 6 faces that are EnvMapSize X EnvMapSize
            float fOmegaP = 4.0 * M_PI / (6.0 * envMapSize * envMapSize);
            // Original paper suggest biasing the mip to improve the results
            float fMipBias = 1.0f;
            float fMipLevel = max(0.5 * log2(fOmegaS / fOmegaP) + fMipBias, 0.0f);
            vPrefilteredColor += EnvironmentCubemapTexture.SampleLevel(LinearSamplerWrap, vLight, fMipLevel).rgb * fNdotL;
            fTotalWeight += fNdotL;
        }
    }
}

