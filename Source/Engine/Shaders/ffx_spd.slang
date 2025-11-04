#define FFX_GPU 1
#define FFX_HLSL 1
#define FFX_HLSL_SM         (62)
#pragma warning(disable: 15205)
#pragma warning(disable: 41000)

// struct ffx_spd_input
// {
//     float mips;
//     float numWorkGroups;
//     float2 workGroupOffset;
// #if GPU_RESOURCE
//     // add missing Texture input here
// #endif
// };
// 
// [Global]
// ParameterBlock<ffx_spd_input> input;


// SPD pass
// SRV  0 : SPD_InputDownsampleSrc          : r_input_downsample_src
// UAV  0 : SPD_InternalGlobalAtomic        : rw_internal_global_atomic
// UAV  1 : SPD_InputDownsampleSrcMidMip    : rw_input_downsample_src_mid_mip
// UAV  2 : SPD_InputDownsampleSrcMips      : rw_input_downsample_src_mips
// CB   0 : cbSPD

#define FFX_SPD_BIND_SRV_INPUT_DOWNSAMPLE_SRC               0

#define FFX_SPD_BIND_UAV_INTERNAL_GLOBAL_ATOMIC             0
#define FFX_SPD_BIND_UAV_INPUT_DOWNSAMPLE_SRC_MID_MIPMAP    1
#define FFX_SPD_BIND_UAV_INPUT_DOWNSAMPLE_SRC_MIPS          2

#define FFX_SPD_BIND_CB_SPD                                 0


#include "fidelityfx/spd/ffx_spd_callbacks_slang.h"
#include "fidelityfx/spd/ffx_spd_downsample.h"

#define FFX_SPD_THREAD_GROUP_WIDTH 256
#define FFX_SPD_THREAD_GROUP_HEIGHT 1
#define FFX_SPD_THREAD_GROUP_DEPTH 1

[shader("compute")]
[numthreads(FFX_SPD_THREAD_GROUP_WIDTH, FFX_SPD_THREAD_GROUP_HEIGHT, FFX_SPD_THREAD_GROUP_DEPTH)]
void main(uint3 workGroupID : SV_GroupID, uint threadGroupIndex : SV_GroupIndex)
{
    DOWNSAMPLE(threadGroupIndex, workGroupID);
}

