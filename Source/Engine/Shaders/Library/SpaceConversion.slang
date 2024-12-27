float LinearEyeDepth(float inDepth, float4 cameraZBufferParams)
{
    return 1.0 / (cameraZBufferParams.z * inDepth + cameraZBufferParams.w);
}
