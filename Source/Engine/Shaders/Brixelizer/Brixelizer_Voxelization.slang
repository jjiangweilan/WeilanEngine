#if CONFIG
name: "Brixelizer/Voxelization"
#endif

struct DispatchInfo
{
    float triangleCount;
}

struct Input
{
[[DynamicBuffer]]
ConstantBuffer<DispatchInfo> dispatchInfo;
StructuredBuffer<float3> vertexBuffer;
StructuredBuffer<uint3> triangleIndices;
}

ParameterBlock<Input> params;

[[shader("compute")]]
void Brixelizer_Voxelization(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint triangleIndex = dispatchThreadID.x;
    uint3 triangleIndices = params.triangleIndices[triangleIndex];
}

