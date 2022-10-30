#pragma once
#include "Core/Graphics/Mesh.hpp"
#include <glm/gtx/intersect.hpp>
namespace Engine
{
    struct Ray
    {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    bool RayMeshIntersection(Ray ray, RefPtr<Mesh> mesh, glm::mat4 transform, float& distance);
    bool RayMeshIntersection(Ray ray, RefPtr<Mesh> mesh, glm::mat4 transform, float& distance, glm::vec3& p0, glm::vec3& p1, glm::vec3& p2);

}
