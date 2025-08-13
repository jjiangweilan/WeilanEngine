#pragma once
#include "Core/Graphics/Mesh.hpp"
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

bool RayVsMesh(Ray ray, RefPtr<Submesh> mesh, glm::mat4 transform, float& distance);

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
    Ray ray, RefPtr<Submesh> mesh, glm::mat4 transform, float& distance, glm::vec3& p0, glm::vec3& p1, glm::vec3& p2
);

bool RayVsAABB(const Ray& r, const AABB& aabb, float& t);

