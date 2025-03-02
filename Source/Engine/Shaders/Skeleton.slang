#define MAX_BONE_SIZE 256
struct Skeleton
{
    float4x4 bones[MAX_BONE_SIZE];

    float4x4 GetSkeletonMatrix(float4 boneWeights)
    {
        float4x4 boneMatrix = float4x4(0);
        for(int i = 0; i < 4; ++i)
        {
            float boneWeight = boneWeights[i];
            if (boneWeight != 0)
            {
                int boneIndex = int(boneWeight * 0.1);
                float weight = boneWeight - boneIndex * 10;
                boneMatrix += bones[boneIndex] * weight;
            }
        }

        return boneMatrix;
    }
};

