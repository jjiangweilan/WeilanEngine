import WeilanEngine;

struct PerMaterial
{
#if GPU_RESOURCE
[format("rgba32f")]
RWTexture2D<float4> outputTex;
#endif
}

[Material]
ParameterBlock<PerMaterial> perMaterial;

[shader("compute")]
[numthreads(8,8,1)]
void FragmentMain(uint3 id : SV_DispatchThreadID)
{
    perMaterial.outputTex[id.xy] = float4(0.5);
}
