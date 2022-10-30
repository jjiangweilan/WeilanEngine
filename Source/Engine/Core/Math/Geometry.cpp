#include "Geometry.hpp"
#include <glm/gtx/intersect.hpp>

namespace Engine
{
    bool RayMeshIntersection(Ray ray, RefPtr<Mesh> mesh, glm::mat4 transform, float& distance)
    {
        glm::vec2 bary;
        auto& vertexDesc = mesh->GetVertexDescription();
        uint16_t* indices = (uint16_t*)vertexDesc.index.data.data();
        glm::vec3* positions = (glm::vec3*)vertexDesc.position.data.data();
        for(int i = 0; i < vertexDesc.index.count; i+=3)
        {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];
            glm::vec4 p0 = transform * glm::vec4(positions[i0], 1);
            glm::vec4 p1 = transform * glm::vec4(positions[i1], 1);
            glm::vec4 p2 = transform * glm::vec4(positions[i2], 1);
            if(glm::intersectRayTriangle(ray.origin, ray.direction, glm::vec3(p0 / p0.w), glm::vec3(p1 / p1.w), glm::vec3(p2 / p2.w), bary, distance))
            {
                return true;
            }
        }
        return false;
    }

    bool RayMeshIntersection(Ray ray, RefPtr<Mesh> mesh, glm::mat4 transform, float& distance, glm::vec3& outP0, glm::vec3& outP1, glm::vec3& outP2)
    {
        glm::vec2 bary;
        auto& vertexDesc = mesh->GetVertexDescription();
        uint16_t* indices = (uint16_t*)vertexDesc.index.data.data();
        glm::vec3* positions = (glm::vec3*)vertexDesc.position.data.data();
        for(int i = 0; i < vertexDesc.index.count; i+=3)
        {
            uint32_t i0 = indices[i];
            uint32_t i1 = indices[i + 1];
            uint32_t i2 = indices[i + 2];
            glm::vec4 p0 = transform * glm::vec4(positions[i0], 1);
            glm::vec4 p1 = transform * glm::vec4(positions[i1], 1);
            glm::vec4 p2 = transform * glm::vec4(positions[i2], 1);
            if(glm::intersectRayTriangle(ray.origin, ray.direction, glm::vec3(p0 / p0.w), glm::vec3(p1 / p1.w), glm::vec3(p2 / p2.w), bary, distance))
            {
                outP0 = positions[i0];
                outP1 = positions[i1];
                outP2 = positions[i2];
                return true;
            }
        }
        return false;
    }
}
