#pragma once
#include "Engine/Runtime/Object/Graphics/Mesh.hpp"
#include <glm/gtx/intersect.hpp>

struct Ray
{
    glm::vec3 origin;
    glm::vec3 direction;
};

struct Frustum
{
    struct CornersOnly
    {};
    Frustum() = default;
    Frustum(const glm::mat4& viewProjection);
    Frustum(const glm::mat4& viewProjection, CornersOnly);
    std::array<float4, 6> planes;
    std::array<float3, 8> corners;
};

struct Triangle
{
    float3 p0;
    float3 p1;
    float3 p2;

    Triangle Transform(const float4x4& model)
    {
        Triangle retval{
            model * float4(p0, 1.0),
            model * float4(p1, 1.0),
            model * float4(p2, 1.0),
        };

        return retval;
    }
};

struct Quad
{
    Quad(const float3& p0, const float3& p1, const float3& p2, const float3& p3) : p0(p0), p1(p1), p2(p2), p3(p3) {}
    Quad() { SetSize(1.0); }
    Quad(const Quad& other) = default;
    Quad& operator=(const Quad& other) = default;

    float3 p0;
    float3 p1;
    float3 p2;
    float3 p3;

    void SetSize(float size)
    {
        float extent = size / 2.0f;

        p0 = float3(extent, 0, extent);
        p1 = float3(-extent, 0, extent);
        p2 = float3(extent, 0, -extent);
        p3 = float3(-extent, 0, -extent);
    }

    Quad Transform(const float4x4& model) const
    {
        Quad retval{
            model * float4(p0, 1.0),
            model * float4(p1, 1.0),
            model * float4(p2, 1.0),
            model * float4(p3, 1.0),
        };

        return retval;
    }
};

struct Box
{
    Box() { SetSize(1.0); }
    Box(const Box& other) = default;
    Box(const float3& position, const float3& scale, const glm::quat& rotation)
    {
        SetSize(scale);
        auto m = glm::translate(glm::float4x4(1.0), position) * glm::mat4_cast(rotation);
        *this = Transform(m);
    }
    Box& operator=(const Box& other) = default;

    // 8 corner points of the box
    float3 points[8];

    void SetSize(const float3& size)
    {
        float3 extent = size / 2.0f;

        // Define corners: (-x,-y,-z) .. (x,y,z)
        points[0] = float3(-extent.x, -extent.y, -extent.z);
        points[1] = float3(extent.x, -extent.y, -extent.z);
        points[2] = float3(-extent.x, extent.y, -extent.z);
        points[3] = float3(extent.x, extent.y, -extent.z);
        points[4] = float3(-extent.x, -extent.y, extent.z);
        points[5] = float3(extent.x, -extent.y, extent.z);
        points[6] = float3(-extent.x, extent.y, extent.z);
        points[7] = float3(extent.x, extent.y, extent.z);
    }

    void SetSize(float size)
    {
        float extent = size / 2.0f;

        SetSize(float3(size, size, size));
    }

    Box Transform(const float4x4& model) const
    {
        Box retval;
        for (int i = 0; i < 8; ++i)
        {
            retval.points[i] = model * float4(points[i], 1.0);
        }

        return retval;
    }

    int GetTriangleCount() const { return 12; }

    Triangle GetTriangle(int idx) const;
};

struct Plane
{
    float3 n;
    float w;
};

struct Circle
{
    float2 center;
    float radius;
};

struct Quad2D
{
    float2 min;
    float2 max;
};

bool RayVsPlane(const Ray& ray, const Plane& plane, float& distance);
bool RayVsQuad(const Ray& ray, const Quad& quad, float& distance);
bool RayVsBox(const Ray& ray, const Box& quad, float& distance);
bool RayVsTriangle(const Ray& ray, const Triangle& triangle, float& distance);
bool RayVsMesh(const Ray& ray, RefPtr<Submesh> mesh, glm::mat4 transform, float& distance);

/**
 * @brief Checks for an intersection between a ray and a submesh, returning details of the intersected triangle.
 *
 * This function transforms the ray using the provided transformation and tests for intersection with the specified
 * submesh. On a successful hit, the distance from the ray origin to the intersection point is output, along with the
 * vertices of the triangle that was intersected.
 *
 * @param ray The ray in world space defined by its origin and direction.
 * @param mesh A reference pointer to the submesh to test for intersection.
 * @param transform The transformation matrix applied to the mesh's vertices.
 * @param distance On success, outputs the distance from the ray origin to the point of intersection.
 * @param p0 On success, outputs one vertex of the intersected triangle.
 * @param p1 On success, outputs the second vertex of the intersected triangle.
 * @param p2 On success, outputs the third vertex of the intersected triangle.
 * @return True if an intersection is found, false otherwise.
 */
bool RayVsMesh(
    const Ray& ray,
    RefPtr<Submesh> mesh,
    glm::mat4 transform,
    float& distance,
    glm::vec3& p0,
    glm::vec3& p1,
    glm::vec3& p2
);

bool RayVsAABB(const Ray& r, const AABB& aabb, float& t);
bool CircleVsQuad2D(const Circle& circle, const Quad2D& quad);
bool AABBVsFrustum(const AABB& aabb, const Frustum& frustum);
bool AABBVsFrustum_XZPlane(const AABB& aabb, const Frustum& frustum);
