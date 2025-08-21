#include "Geometry.hpp"
#include <glm/gtx/intersect.hpp>

bool RayVsQuad(const Ray& ray, const Quad& quad, float& distance)
{
    float2 bary;
    if (glm::intersectRayTriangle(ray.origin, ray.direction, quad.p0, quad.p1, quad.p2, bary, distance) ||
        glm::intersectRayTriangle(ray.origin, ray.direction, quad.p1, quad.p2, quad.p3, bary, distance))
    {
        return true;
    }

    return false;
}

bool RayVsBox(const Ray& ray, const Box& box, float& distance)
{
    for (int i = 0; i < 6; ++i)
    {
        if (RayVsQuad(ray, box.faces[i], distance))
            return true;
    }

    return false;
}

bool RayVsTriangle(const Ray& ray, const Triangle& triangle, float& distance)
{
    float2 bary;
    if (glm::intersectRayTriangle(ray.origin, ray.direction, triangle.p0, triangle.p1, triangle.p2, bary, distance))
    {
        return true;
    }

    return false;
}

bool RayVsMesh(const Ray& ray, RefPtr<Submesh> mesh, glm::mat4 transform, float& distance)
{
    glm::vec2 bary;
    uint16_t* indices = (uint16_t*)mesh->GetIndexBufferData();
    glm::vec3* positions = (glm::vec3*)mesh->GetVertexBufferData();
    for (int i = 0; i < mesh->GetIndexCount(); i += 3)
    {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];
        glm::vec4 p0 = transform * glm::vec4(positions[i0], 1);
        glm::vec4 p1 = transform * glm::vec4(positions[i1], 1);
        glm::vec4 p2 = transform * glm::vec4(positions[i2], 1);
        if (glm::intersectRayTriangle(
                ray.origin,
                ray.direction,
                glm::vec3(p0 / p0.w),
                glm::vec3(p1 / p1.w),
                glm::vec3(p2 / p2.w),
                bary,
                distance
            ))
        {
            return true;
        }
    }
    return false;
}

bool RayVsMesh(
    const Ray& ray,
    RefPtr<Submesh> mesh,
    glm::mat4 transform,
    float& distance,
    glm::vec3& outP0,
    glm::vec3& outP1,
    glm::vec3& outP2
)
{
    glm::vec2 bary;
    uint16_t* indices = (uint16_t*)mesh->GetIndexBufferData();
    glm::vec3* positions = (glm::vec3*)mesh->GetIndexBufferData();
    for (int i = 0; i < mesh->GetIndexCount(); i += 3)
    {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];
        glm::vec4 p0 = transform * glm::vec4(positions[i0], 1);
        glm::vec4 p1 = transform * glm::vec4(positions[i1], 1);
        glm::vec4 p2 = transform * glm::vec4(positions[i2], 1);
        if (glm::intersectRayTriangle(
                ray.origin,
                ray.direction,
                glm::vec3(p0 / p0.w),
                glm::vec3(p1 / p1.w),
                glm::vec3(p2 / p2.w),
                bary,
                distance
            ))
        {
            outP0 = positions[i0];
            outP1 = positions[i1];
            outP2 = positions[i2];
            return true;
        }
    }
    return false;
}

Frustum::Frustum(const glm::mat4& vp)
{
    auto row0 = glm::row(vp, 0);
    auto row1 = glm::row(vp, 1);
    auto row2 = glm::row(vp, 2);
    auto row3 = glm::row(vp, 3);

    // Left plane
    planes[0] = row3 + row0;
    // Right plane
    planes[1] = row3 - row0;
    // Bottom plane
    planes[2] = row3 + row1;
    // Top plane
    planes[3] = row3 - row1;
    // Near plane
    planes[4] = row2; // z ranges from 0 to 1
    // Far plane
    planes[5] = row3 - row2;

    for (int i = 0; i < 6; ++i)
    {
        float length = glm::length(glm::vec3(planes[i]));
        planes[i] /= length;
    }

    std::array<glm::vec4, 8> corners = {
        glm::vec4(-1, -1, 0, 1),
        glm::vec4(1, -1, 0, 1),
        glm::vec4(1, 1, 0, 1),
        glm::vec4(-1, 1, 0, 1),
        glm::vec4(-1, -1, 1, 1),
        glm::vec4(1, -1, 1, 1),
        glm::vec4(1, 1, 1, 1),
        glm::vec4(-1, 1, 1, 1)
    };

    auto invViewProj = glm::inverse(vp);
    int i = 0;
    for (auto& v : corners)
    {
        v = invViewProj * v;
        v /= v.w;

        this->corners[i++] = v;
    }
}

Frustum::Frustum(const glm::mat4& vp, CornersOnly)
{
    std::array<glm::vec4, 8> corners = {
        glm::vec4(-1, -1, 0, 1),
        glm::vec4(1, -1, 0, 1),
        glm::vec4(1, 1, 0, 1),
        glm::vec4(-1, 1, 0, 1),
        glm::vec4(-1, -1, 1, 1),
        glm::vec4(1, -1, 1, 1),
        glm::vec4(1, 1, 1, 1),
        glm::vec4(-1, 1, 1, 1)
    };

    auto invViewProj = glm::inverse(vp);
    int i = 0;
    for (auto& v : corners)
    {
        v = invViewProj * v;
        v /= v.w;

        this->corners[i++] = v;
    }
}

bool RayVsAABB(const Ray& r, const AABB& aabb, float& t)
{
    glm::vec3 lb = aabb.min;
    glm::vec3 rt = aabb.max;
    glm::vec3 dirfrac;
    // r.dir is unit direction vector of ray
    dirfrac.x = 1.0f / r.direction.x;
    dirfrac.y = 1.0f / r.direction.y;
    dirfrac.z = 1.0f / r.direction.z;
    // lb is the corner of AABB with minimal coordinates - left bottom, rt is maximal corner
    // r.org is origin of ray
    float t1 = (lb.x - r.origin.x) * dirfrac.x;
    float t2 = (rt.x - r.origin.x) * dirfrac.x;
    float t3 = (lb.y - r.origin.y) * dirfrac.y;
    float t4 = (rt.y - r.origin.y) * dirfrac.y;
    float t5 = (lb.z - r.origin.z) * dirfrac.z;
    float t6 = (rt.z - r.origin.z) * dirfrac.z;

    float tmin = glm::max(glm::max(glm::min(t1, t2), glm::min(t3, t4)), glm::min(t5, t6));
    float tmax = glm::min(glm::min(glm::max(t1, t2), glm::max(t3, t4)), glm::max(t5, t6));

    // if tmax < 0, ray (line) is intersecting AABB, but the whole AABB is behind us
    if (tmax < 0)
    {
        t = tmax;
        return false;
    }

    // if tmin > tmax, ray doesn't intersect AABB
    if (tmin > tmax)
    {
        t = tmax;
        return false;
    }

    t = tmin;
    return true;
}
