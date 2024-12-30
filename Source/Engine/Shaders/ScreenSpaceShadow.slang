#include "Library/PerScene.hlsl"

ParameterBlock<PerScene> perScene;

struct PerMaterial
{
#if GPU_RESOURCE
RWTexture2D outputTex;
#endif
}

[shader("compute")]
[numthreads(8,8,1)]
void FragmentMain(uint3 id : SV_DispatchThreadID)
{
}
