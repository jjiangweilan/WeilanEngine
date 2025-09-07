#include "bend_sss_gpu.hlsl"

[shader("compute")]
[numthreads(8,8,1)]
void Main(int3 groupID : SV_GroupID, int3 groupThreadID : SV_GroupThreadID)
{
    DispatchParameters parameters;
    WriteScreenSpaceShadow(parameters, groupID, groupThreadID);
}
