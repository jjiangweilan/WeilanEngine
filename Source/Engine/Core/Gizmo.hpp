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
    GizmoBase()
    {
        carrier = GetActiveCarrier();
    }
    // a gizmos can be associated with a GameObject, Gizmos use static pattern so the associated carrier is recorded
    // besides OnDrawGizmos by the caller
    static void SetActiveCarrier(GameObject* carrier);
    static void ClearActiveCarrier();

    static Shader* GetBillboardShader();
    virtual void Draw(Gfx::CommandBuffer& cmd) = 0;
    virtual AABB GetAABB() = 0;

    GameObject* GetCarrier()
    {
        return carrier;
    }

private:
    GameObject* carrier;
    static GameObject*& GetActiveCarrier();
};

class Gizmos
{
public:
    static void DrawMesh(Mesh& mesh, int submeshIndex, Shader* shader, const glm::mat4& modelMatrix);
    static void DrawLight(const glm::vec3& position);
    static void DispatchAllDiszmos(Gfx::CommandBuffer& cmd);

    int GetSize()
    {
        return gizmos.size();
    }

    static bool RayVsAABB(const Ray& r, const AABB& aabb, float& t);
    static void PickGizmos(const Ray& ray, std::vector<GameObject*>& result);
    static void ClearAllRegisteredGizmos()
    {
        GetSingleton().gizmos.clear();
    }

    template <std::derived_from<GizmoBase> T, class... Args>
    void Add(Args&&... args)
    {
        gizmos.push_back(T(std::forward<Args>(args)...));
    }

    void Clear()
    {
        gizmos.clear();
    }

private:
    static Gizmos& GetSingleton();
    std::vector<std::unique_ptr<GizmoBase>> gizmos;
};
