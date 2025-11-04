import WeilanEngine;

#define FFX_GPU 1
#define FFX_HLSL 1
#define FFX_HALF 0
#define FFX_HLSL_SM         (62)

#include "fidelityfx/ffx_core.h"
#include "fidelityfx/spd/ffx_spd.h"

struct ffx_spd_input
{
    float mips;
    float numWorkGroups;
    float2 workGroupOffset;
};

[Global]
ParameterBlock<ffx_spd_input> input;

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 workGroupID : SV_GroupID, uint threadGroupIndex : SV_GroupIndex)
{
    int mips = (int)input.mips;
    int numWorkGroups = (int)input.numWorkGroups;
    float2 workGroupOffset = (float2)input.workGroupOffset;
    SpdDownsample(workGroupID.xy, threadGroupIndex, mips, numWorkGroups, workGroupID.z, workGroupOffset);
}

