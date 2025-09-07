import WeilanEngine;

#include "./bend_sss_gpu.hlsl"
#include "./ContactShadowParameters.hlsl"

[[Material]]
ParameterBlock<ContactShadowParameters> props;

[[vk_push_constant]]
ContactShadowPushConstant ps;

// Previous (8,8,1) layout broke the math using inGroupID.yz * WAVE_SIZE.
[shader("compute")]
[numthreads(64,1,1)]
void Main(uint3 groupID : SV_GroupID, uint groupThreadID : SV_GroupThreadID)
{
    DispatchParameters parameters = DispatchParameters(props, ps.waveOffset);

    // Values set by host (see ContactShadowPass) via raw parameter buffer or push constants (engine dependent).
    WriteScreenSpaceShadow(parameters, groupID, groupThreadID);
}
