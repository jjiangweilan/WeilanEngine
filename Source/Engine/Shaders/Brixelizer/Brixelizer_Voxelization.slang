#if CONFIG
name: "Brixelizer/Voxelization"
#endif

StructuredBuffer<float3>.Handle vertexPositionBuffers;
StructuredBuffer<uint3>.Handle triangleIndices;

[[shader("compute")]]
void Brixelizer_Voxelization(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint triangleIndex = dispatchThreadID.x;
    uint3 triangleIndices = triangleIndices[triangleIndex];

    vertexPositionBuffers[triangleIndices.x];
}

