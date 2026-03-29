
#include "Library/PerScene.hlsl"
#include "GPUDriven/GPUDrivenStructures.hlsl"

public struct RayHitVertexData
{
    public float3 position;
    public float3 normal;
    public float2 uv;
};

/**
 * Retrieves and interpolates vertex data (position, normal, uv) for a ray hit.
 * 
 * @param globalBuffer The global byte address buffer containing geometry data.
 * @param gpuObjectOffsets The buffer containing offsets for each GPU object.
 * @param instanceID The ID of the instance hit (from q.CommittedInstanceID()).
 * @param geometryIndex The index of the geometry within the instance (from q.CommittedGeometryIndex()).
 * @param primitiveIndex The index of the triangle within the geometry (from q.CommittedPrimitiveIndex()).
 * @param bary2 The 2D barycentric coordinates (from q.CommittedTriangleBarycentrics()).
 * @return The interpolated vertex data at the hit point.
 */
public RayHitVertexData GetHitVertexData(
    ByteAddressBuffer globalBuffer,
    StructuredBuffer<uint> gpuObjectOffsets,
    uint instanceID, 
    uint geometryIndex, 
    uint primitiveIndex, 
    float2 bary2)
{
    // 1. Calculate the 3D barycentric coordinates
    float3 bary3 = float3(1.0 - bary2.x - bary2.y, bary2.x, bary2.y);

    // 2. Locate the object and its render data
    uint objectOffset = gpuObjectOffsets[instanceID];
    GpuObject obj = globalBuffer.Load<GpuObject>(objectOffset);
    
    // GeometryIndex maps directly to the array of render data for this object
    uint renderDataOffset = obj.pRenderDataOffset + geometryIndex * 16; // GpuRenderData is 16 bytes
    GpuRenderData renderData = globalBuffer.Load<GpuRenderData>(renderDataOffset);
    
    // Load the geometry header which contains offsets for indices and vertices
    GpuGeometry geometry = globalBuffer.Load<GpuGeometry>(renderData.geometryOffset);

    // 3. Load the 3 vertex indices for the hit triangle
    uint indexByteOffset = geometry.indexOffset + (primitiveIndex * 3 * 4);
    uint i0 = globalBuffer.Load<uint>(indexByteOffset);
    uint i1 = globalBuffer.Load<uint>(indexByteOffset + 4);
    uint i2 = globalBuffer.Load<uint>(indexByteOffset + 8);

    // 4. Load the vertex positions
    GpuGeometryPositionData p0 = globalBuffer.Load<GpuGeometryPositionData>(geometry.positionOffset + i0 * sizeof(GpuGeometryPositionData));
    GpuGeometryPositionData p1 = globalBuffer.Load<GpuGeometryPositionData>(geometry.positionOffset + i1 * sizeof(GpuGeometryPositionData));
    GpuGeometryPositionData p2 = globalBuffer.Load<GpuGeometryPositionData>(geometry.positionOffset + i2 * sizeof(GpuGeometryPositionData));

    // 5. Load the vertex attributes (normal, tangent, uv)
    SceneObjectAttributeData a0 = globalBuffer.Load<SceneObjectAttributeData>(geometry.attributeOffset + i0 * sizeof(SceneObjectAttributeData));
    SceneObjectAttributeData a1 = globalBuffer.Load<SceneObjectAttributeData>(geometry.attributeOffset + i1 * sizeof(SceneObjectAttributeData));
    SceneObjectAttributeData a2 = globalBuffer.Load<SceneObjectAttributeData>(geometry.attributeOffset + i2 * sizeof(SceneObjectAttributeData));

    // 6. Interpolate and return
    RayHitVertexData result;
    result.position = p0.position * bary3.x + p1.position * bary3.y + p2.position * bary3.z;
    result.normal   = normalize(a0.normal * bary3.x + a1.normal * bary3.y + a2.normal * bary3.z);
    result.uv       = a0.uv * bary3.x + a1.uv * bary3.y + a2.uv * bary3.z;

    return result;
}

/**
 * Traces a visibility (shadow) ray using inline ray tracing.
 */
public bool TraceVisibilityRay(
    RaytracingAccelerationStructure bvh, 
    float3 origin, 
    float3 direction, 
    float tMin = 0.001, 
    float tMax = 10000.0)
{
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = tMin;
    ray.TMax = tMax;

    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;
    q.TraceRayInline(bvh, RAY_FLAG_NONE, 0xFF, ray);
    q.Proceed();

    return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

/**
 * Offsets a ray origin from a surface to prevent self-intersection.
 */
public float3 OffsetRay(float3 p, float3 n)
{
    const float origin = 1.0f / 32.0f;
    const float float_scale = 1.0f / 65536.0f;
    const float int_scale = 256.0f;

    int3 of_i = int3((int)(int_scale * n.x), (int)(int_scale * n.y), (int)(int_scale * n.z));

    float3 p_i = float3(
        asfloat(asint(p.x) + (p.x < 0 ? -of_i.x : of_i.x)),
        asfloat(asint(p.y) + (p.y < 0 ? -of_i.y : of_i.y)),
        asfloat(asint(p.z) + (p.z < 0 ? -of_i.z : of_i.z))
    );

    return float3(
        abs(p.x) < origin ? p.x + float_scale * n.x : p_i.x,
        abs(p.y) < origin ? p.y + float_scale * n.y : p_i.y,
        abs(p.z) < origin ? p.z + float_scale * n.z : p_i.z
    );
}
