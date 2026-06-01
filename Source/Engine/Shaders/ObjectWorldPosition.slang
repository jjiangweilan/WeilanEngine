import Library.PerScene;

float4x4 GetObjectSkeletonMatrix(ParameterBlock<PerScene> perScene, uint skeletonOffset, float4 boneWeights, out bool hasWeight)
{
    float4x4 skeletonMatrix = float4x4(0);
    hasWeight = false;
    for (int i = 0; i < 4; ++i)
    {
        float boneWeight = boneWeights[i];
        if (boneWeight != 0)
        {
            int boneIndex = int(boneWeight * 0.1);
            float weight = boneWeight - boneIndex * 10;
            skeletonMatrix += perScene.LoadDynamicData<float4x4>(skeletonOffset, boneIndex) * weight;

            if (weight > 1e-4)
                hasWeight = true;
        }
    }

    return skeletonMatrix;
}

float4x4 GetObjectModelMatrix(
    ParameterBlock<PerScene> perScene,
    float4x4 modelMatrix,
    uint skeletonOffset,
    float4 boneWeights,
    out bool hasBoneWeight
)
{
    hasBoneWeight = false;
    if (skeletonOffset == 0xFFFFFFFF)
        return modelMatrix;

    float4x4 skeletonMatrix = GetObjectSkeletonMatrix(perScene, skeletonOffset, boneWeights, hasBoneWeight);
    return hasBoneWeight ? mul(modelMatrix, skeletonMatrix) : modelMatrix;
}
