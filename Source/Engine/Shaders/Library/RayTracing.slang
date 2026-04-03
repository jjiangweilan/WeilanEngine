
import Library.PerScene;

struct RayHitVertexData
{
    float3 position;
    float3 normal;
    float2 uv;

    RayHitVertexData TransformToWorld(float4x4 modelMatrix, float3x3 invTspModelMatrix)
    {
        RayHitVertexData rayHitVertexData;

        rayHitVertexData.position = mul(modelMatrix, float4(this.position, 1.0)).xyz;
        rayHitVertexData.normal = normalize(mul(invTspModelMatrix, this.normal));
        rayHitVertexData.uv = this.uv;
        return rayHitVertexData;
    }
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
RayHitVertexData GetHitVertexData(
    ParameterBlock<PerScene> perScene,
    StructuredBuffer<uint> rtObjectOffsets,
    uint instanceID, 
    uint geometryIndex, 
    uint primitiveIndex, 
    float2 bary2)
{
    // 1. Calculate the 3D barycentric coordinates
    float3 bary3 = float3(1.0 - bary2.x - bary2.y, bary2.x, bary2.y);

    // 2. Locate the object and its render data
    uint objectOffset = rtObjectOffsets[instanceID];
    ObjectTriangle tri = ObjectTriangle(perScene, objectOffset, geometryIndex, primitiveIndex);

    // 6. Interpolate and return
    RayHitVertexData result;
    result.position = tri.GetPosition(perScene, 0) * bary3.x + tri.GetPosition(perScene, 1) * bary3.y + tri.GetPosition(perScene, 2) * bary3.z;
    result.normal   = normalize(tri.GetNormal(perScene, 0) * bary3.x + tri.GetNormal(perScene, 1) * bary3.y + tri.GetNormal(perScene, 2) * bary3.z);
    result.uv       = tri.GetUV(perScene, 0) * bary3.x + tri.GetUV(perScene, 1) * bary3.y + tri.GetUV(perScene, 2) * bary3.z;

    result = result.TransformToWorld(tri.GetModelMatrix(), (float3x3)tri.GetInvModelMatrix());

    return result;
}

/**
 * Traces a visibility (shadow) ray using inline ray tracing.
 */
bool TraceVisibilityRay(
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
    q.TraceRayInline(bvh, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, ray);
    q.Proceed();

    return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

/**
 * Offsets a ray origin from a surface to prevent self-intersection.
 */
float3 OffsetRay(float3 p, float3 n)
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
