#ifndef SPACE_CONVERSION_INCLUDED
#define SPACE_CONVERSION_INCLUDED
float LinearEyeDepth(float inDepth, vec4 cameraZBufferParams)
{
    return 1.0 / (cameraZBufferParams.z * inDepth + cameraZBufferParams.w);
}
#endif
