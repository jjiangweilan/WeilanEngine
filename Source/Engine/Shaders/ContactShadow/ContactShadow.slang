import WeilanEngine;
#include "./bend_sss_gpu.hlsl"

[[Material]]
ParameterBlock<ContactShadowParameters> props;

[[vk_push_constant]]
ContactShadowPushConstant ps;

[shader("compute")]
[numthreads(64,1,1)]
void Main(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID)
{
    DispatchParameters parameters = DispatchParameters(props, ps.waveOffset);

    // Values set by host (see ContactShadowPass) via raw parameter buffer or push constants (engine dependent).
    WriteScreenSpaceShadow(parameters, groupID, groupThreadID.x);
}
