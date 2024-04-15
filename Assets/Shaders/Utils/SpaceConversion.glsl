#ifndef SPACE_CONVERSION_INCLUDED
#define SPACE_CONVERSION_INCLUDED
float LinearEyeDepth(float inDepth)
{
    return 1.0 / (scene.cameraZBufferParams.z * inDepth + scene.cameraZBufferParams.w);
}
#endif
