#pragma once
#include "Core/Math/Geometry.hpp"
#include "Utils/Structs.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <variant>
#include <vector>

class Texture;
class Shader;
class GameObject;
namespace Gfx
{
class CommandBuffer;
}

class GizmoBase
{
public:
    static Shader* GetBillboardShader();

    void SetCarrier(GameObject* carrier)
    {
        this->carrier = carrier;
    }

    GameObject* GetCarrier()
    {
        return carrier;
    }

protected:
    GameObject* carrier;
};

class GizmoLight : public GizmoBase
{
public:
    GizmoLight() : position(0){};
    GizmoLight(const glm::vec3& position);
    void Draw(Gfx::CommandBuffer& cmd);
    AABB GetAABB()
    {
        return AABB(position, scale);
    }

private:
    glm::vec3 position;
    const glm::vec3 scale = glm::vec3(0.7f);
    static Texture* GetLightTexture();
};

class GizmoCamera : public GizmoBase
{
public:
    GizmoCamera(){};
    GizmoCamera(float fov, float near, float far, float aspect);

private:
    float fov;
    float near;
    float far;
    float aspect;
};

using GizmoVariant = std::variant<GizmoLight>;

class Gizmos
{
public:
    static void Draw(Mesh& mesh, int submeshIndex, Shader* shader);
    static void DrawLight(const glm::vec3& position);

    void AssignCarrier(GameObject* carrier, int from, int to)
    {
        for (int i = from; i < to; ++i)
        {
            std::visit([carrier](auto&& val) { val.SetCarrier(carrier); }, gizmos[i]);
        }
    }

    int GetSize()
    {
        return gizmos.size();
    }

    static bool RayVsAABB(const Ray& r, const AABB& aabb, float& t)
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

    void PickGizmos(const Ray& ray, std::vector<GameObject*>& result)
    {
        result.clear();
        for (auto& g : gizmos)
        {
            std::visit(
                [&result, ray](auto&& val)
                {
                    AABB aabb = val.GetAABB();
                    float t;
                    if (RayVsAABB(ray, aabb, t))
                    {
                        result.push_back(val.GetCarrier());
                    }
                },
                g
            );
        }
    }

    template <std::derived_from<GizmoBase> T, class... Args>
    void Add(Args&&... args)
    {
        gizmos.push_back(T(std::forward<Args>(args)...));
    }

    void Draw(Gfx::CommandBuffer& cmd)
    {
        for (auto& g : gizmos)
        {
            std::visit([&cmd](auto&& val) { val.Draw(cmd); }, g);
        }
    }

    void Clear()
    {
        gizmos.clear();
    }

private:
    std::vector<GizmoVariant> gizmos;
};
