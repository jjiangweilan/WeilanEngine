#pragma once
#include "Core/Graphics/Mesh.hpp"
#include <glm/gtx/intersect.hpp>
namespace Engine::Utils
{
    struct Ray
    {
        glm::vec3 origin;
        glm::vec3 direction;
    };

    bool RayMeshIntersection(Ray ray, RefPtr<Mesh> mesh, glm::mat4 transform, float& distance);
}
