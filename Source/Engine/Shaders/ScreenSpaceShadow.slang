#include "Library/PerScene.hlsl"

ParameterBlock<PerScene> perScene;
[shader("compute")]
[numthreads(8,8,1)]
void FragmentMain(uint3 id : SV_DispatchThreadID)
{
}
