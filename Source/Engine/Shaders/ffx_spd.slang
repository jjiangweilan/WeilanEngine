import WeilanEngine;

#define FFX_GPU 1

#include "fidelityfx/ffx_core.h"
#include "fidelityfx/ffx_core_gpu_common.h"
#include "fidelityfx/spd/ffx_spd.h"

struct ffx_spd_input
{
    uint mips;
    uint numWorkGroups;
    uint slice;
};

[Global]
ParameterBlock<ffx_spd_input> input;

[shader("compute")]
[numthreads(256, 1, 1)]
void main(uint3 workGroupID : SV_GroupID, uint threadGroupIndex : SV_GroupIndex)
{
    int mips = 1;
    int numWorkGroups = 1;
    int slice = 1;
    SpdDownsample(workGroupID.xy, threadGroupIndex, mips, numWorkGroups, slice);
}

