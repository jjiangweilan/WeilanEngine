#include "Geometry.hpp"
#include <glm/gtx/intersect.hpp>

Triangle Box::GetTriangle(int idx) const
{
    if (idx < 0 || idx >= 12)
        return {float3(0, 0, 0), float3(0, 0, 0), float3(0, 0, 0)};

    // returns counter clockwise facing triangle (outward facing for the box)
    static const int faces[6][4] = {
        {0, 2, 6, 4}, // -X
        {1, 5, 7, 3}, // +X
        {0, 4, 5, 1}, // -Y
        {2, 3, 7, 6}, // +Y
        {0, 1, 3, 2}, // -Z
        {4, 6, 7, 5}, // +Z
    };

    int face = idx / 2;
    int tri = idx % 2;

    int a = faces[face][0];
    int b = faces[face][1];
    int c = faces[face][2];
    int d = faces[face][3];

    // Use diagonal (a-c). To ensure outward CCW orientation, reverse the winding from (a,b,c)/(a,c,d)
    if (tri == 0)
        return {points[a], points[c], points[b]};
    else
        return {points[a], points[d], points[c]};
};

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
    // Define faces from 8 points and test against each as a quad
    const int faces[6][4] = {
        {0, 1, 3, 2}, // -Z
        {4, 6, 7, 5}, // +Z
        {0, 2, 6, 4}, // -X
        {1, 5, 7, 3}, // +X
        {0, 4, 5, 1}, // -Y
        {2, 3, 7, 6}  // +Y
    };

    bool hit = false;
    float best = 1e30f;

    for (int i = 0; i < 6; ++i)
    {
        Quad q(box.points[faces[i][0]], box.points[faces[i][1]], box.points[faces[i][2]], box.points[faces[i][3]]);
        float d;
        if (RayVsQuad(ray, q, d))
        {
            if (d < best)
            {
                best = d;
                hit = true;
            }
        }
    }

    if (hit)
    {
        distance = best;
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
    const uint32_t* indices = mesh->GetIndices().data();
    const glm::vec3* positions = mesh->GetPositions().data();
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
    auto* indices = mesh->GetIndices().data();
    const glm::vec3* positions = mesh->GetPositions().data();
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

bool RayVsPlane(const Ray& ray, const Plane& plane, float& distance)
{
    float denom = glm::dot(plane.n, ray.direction);
    if (glm::abs(denom) > 1e-6)
    {
        float t = (plane.w - glm::dot(plane.n, ray.origin)) / denom;
        if (t >= 0)
        {
            distance = t;
            return true;
        }
    }
    return false;
}

bool CircleVsQuad2D(const Circle& circle, const Quad2D& quad)
{
    // Find the closest point on the quad (AABB) to the circle center
    float2 closestPoint;
    closestPoint.x = glm::clamp(circle.center.x, quad.min.x, quad.max.x);
    closestPoint.y = glm::clamp(circle.center.y, quad.min.y, quad.max.y);

    // Calculate the distance between the circle center and the closest point
    float2 distance = circle.center - closestPoint;
    float distanceSquared = glm::dot(distance, distance);

    // Circle intersects if the distance is less than or equal to the radius
    return distanceSquared <= (circle.radius * circle.radius);
}

bool AABBVsFrustum(const AABB& aabb, const Frustum& frustum)
{
    const glm::vec3& vmin = aabb.min;
    const glm::vec3& vmax = aabb.max;

    for (size_t i = 0; i < 6; ++i)
    {
        const glm::vec4& g = frustum.planes[i];
        if ((glm::dot(g, glm::vec4(vmin.x, vmin.y, vmin.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmax.x, vmin.y, vmin.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmin.x, vmax.y, vmin.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmax.x, vmax.y, vmin.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmin.x, vmin.y, vmax.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmax.x, vmin.y, vmax.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmin.x, vmax.y, vmax.z, 1.0f)) < 0.0) &&
            (glm::dot(g, glm::vec4(vmax.x, vmax.y, vmax.z, 1.0f)) < 0.0))
        {
            // Completely outside the frustum
            return false;
        }
    }

    return true;
}

bool AABBVsFrustum_XZPlane(const AABB& aabb, const Frustum& frustum)
{
    // Project Frustum corners to 2D (XZ)
    glm::vec2 f_corners[8];
    for (int i = 0; i < 8; ++i)
    {
        f_corners[i] = glm::vec2(frustum.corners[i].x, frustum.corners[i].z);
    }

    // Calculate AABB of the Frustum in 2D
    glm::vec2 f_min = f_corners[0];
    glm::vec2 f_max = f_corners[0];
    for (int i = 1; i < 8; ++i)
    {
        f_min = glm::min(f_min, f_corners[i]);
        f_max = glm::max(f_max, f_corners[i]);
    }

    // Check AABB vs Frustum AABB (SAT on X and Z axes)
    if (f_max.x < aabb.min.x || f_min.x > aabb.max.x ||
        f_max.y < aabb.min.z || f_min.y > aabb.max.z)
    {
        return false;
    }

    // SAT on Frustum edges
    // Edges indices
    int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, // Near plane
        {4, 5}, {5, 6}, {6, 7}, {7, 4}, // Far plane
        {0, 4}, {1, 5}, {2, 6}, {3, 7}  // Connecting
    };

    // AABB corners in 2D
    glm::vec2 a_corners[4] = {
        glm::vec2(aabb.min.x, aabb.min.z),
        glm::vec2(aabb.max.x, aabb.min.z),
        glm::vec2(aabb.max.x, aabb.max.z),
        glm::vec2(aabb.min.x, aabb.max.z)
    };

    for (int i = 0; i < 12; ++i)
    {
        glm::vec2 p1 = f_corners[edges[i][0]];
        glm::vec2 p2 = f_corners[edges[i][1]];
        glm::vec2 edge = p2 - p1;

        // Skip degenerate edges
        if (glm::dot(edge, edge) < 1e-6f)
            continue;

        // Normal (perpendicular) to the edge
        glm::vec2 normal(-edge.y, edge.x);

        // Project Frustum onto normal
        float f_min_proj = glm::dot(normal, f_corners[0]);
        float f_max_proj = f_min_proj;
        for (int k = 1; k < 8; ++k)
        {
            float proj = glm::dot(normal, f_corners[k]);
            f_min_proj = glm::min(f_min_proj, proj);
            f_max_proj = glm::max(f_max_proj, proj);
        }

        // Project AABB onto normal
        float a_min_proj = glm::dot(normal, a_corners[0]);
        float a_max_proj = a_min_proj;
        for (int k = 1; k < 4; ++k)
        {
            float proj = glm::dot(normal, a_corners[k]);
            a_min_proj = glm::min(a_min_proj, proj);
            a_max_proj = glm::max(a_max_proj, proj);
        }

        // Check for separation
        if (f_max_proj < a_min_proj || a_max_proj < f_min_proj)
        {
            return false;
        }
    }

    return true;
}
