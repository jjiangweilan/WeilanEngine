#pragma once
#include "Editor//Gizmos/GizmoBase.hpp"

class Texture;
class GameObject;
namespace Gfx
{
class CommandBuffer;
}

struct InteractiveBox
{
    bool isDragging = false;
};

// a selectable GUI overlay/3D object in scene for editor
class Gizmos
{
public:
    static GizmoBase* DrawInteractiveBox(InteractiveBox& box, const float3& position, float3& size);
    static GizmoBase* DrawMesh(Mesh& mesh, int submeshIndex, ObjPtr<Shader2> shader, const glm::mat4& modelMatrix);
    static GizmoBase* DrawMesh(Mesh& mesh, int submeshIndex, Material* shader, const glm::mat4& modelMatrix);
    static GizmoBase* DrawLight(const glm::vec3& position);
    static GizmoBase* DrawCamera(const glm::vec3& position);
    static void DispatchAllDiszmos(Gfx::CommandBuffer& cmd, Gfx::ShaderResource* perScene);

    int GetSize() { return gizmos.size(); }

    static bool RayVsAABB(const Ray& r, const AABB& aabb, float& t);
    static void PickGizmos(const Ray& ray, std::vector<GameObject*>& result);
    static void ClearAllRegisteredGizmos() { GetSingleton().gizmos.clear(); }

    template <std::derived_from<GizmoBase> T, class... Args>
    void Add(Args&&... args)
    {
        gizmos.push_back(T(std::forward<Args>(args)...));
    }

    void Clear() { gizmos.clear(); }

    static void ResourceCleanup();

private:
    static Gizmos& GetSingleton();
    std::vector<std::unique_ptr<GizmoBase>> gizmos;
};
