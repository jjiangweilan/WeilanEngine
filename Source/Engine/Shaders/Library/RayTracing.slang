
import Library.PerScene;

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

/**
 * Traces a visibility (shadow) ray using inline ray tracing.
 */
bool TraceVisibilityRay(
    RaytracingAccelerationStructure bvh, 
    float3 origin, 
    float3 direction, 
    float tMin = 0, 
    float tMax = 1000.0)
{
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = tMin;
    ray.TMax = tMax;

    RayQuery<RAY_FLAG_CULL_NON_OPAQUE |
             RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;
    q.TraceRayInline(bvh, RAY_FLAG_NONE, 0xFF, ray);
    while(q.Proceed());

    return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}

bool TraceVisibilityRay(
    RaytracingAccelerationStructure bvh, 
    float3 origin, 
    float3 normal,
    float3 direction, 
    float tMin = 0, 
    float tMax = 1000.0)
{
    RayDesc ray;
    ray.Origin = OffsetRay(origin, normal);
    ray.Direction = direction;
    ray.TMin = 0;
    ray.TMax = 1000;

    RayQuery<RAY_FLAG_CULL_NON_OPAQUE |
             RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES |
             RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> q;
    q.TraceRayInline(bvh, RAY_FLAG_NONE, 0xFF, ray);
    while(q.Proceed());

    return q.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
}
