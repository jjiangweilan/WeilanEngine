#pragma once
#include "Core/Math/Geometry.hpp"
#include "Libs/DynamicArray.hpp"
#include "Rendering/Material.hpp"
#include "Rendering/Structs.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <variant>

class Texture;
class GameObject;
namespace Gfx
{
class CommandBuffer;
}

class GizmoBase
{
public:
    GizmoBase() { carrier = GetActiveCarrier(); }
    virtual ~GizmoBase() {}

    /**
     * @brief Set active carrier
     *
     * @param carrier a gizmos can be associated with a GameObject, Gizmos use static pattern so the associated carrier
     * is recorded when GameObject.OnDrawGizmos called
     */
    static void SetActiveCarrier(GameObject* carrier);
    static void ClearActiveCarrier();
    static ObjPtr<Shader2> GetBillboardShader();

    /**
     * @brief Used to prepare drawing
     *
     * @param perScene the scene shader resource of currently rendering scene
     */
    void SetupDraw(Gfx::ShaderResource* perScene) { this->perScene = perScene; }

    /**
     * @brief Record draw commands into cmd
     *
     * @param cmd the commands that will be recorded into
     */
    virtual void Draw(Gfx::CommandBuffer& cmd) = 0;

    /**
     * @brief wether this gizmo should be picked as active
     *
     * @param ray world space view ray
     * @return true if it should be picked
     */
    virtual bool Pick(const Ray& ray) { return false; }

    GameObject* GetCarrier() { return carrier; }

    void AddOnDragCallback(std::function<void(const float2&)> callback) { onDragCallbacks.push_back(callback); }

protected:
    Gfx::ShaderResource* perScene;

    /**
     * @brief on drag callback, pass in the mouse movement in screen space
     */
    std::vector<std::function<void(const float2&)>> onDragCallbacks;

private:
    GameObject* carrier;
    static GameObject*& GetActiveCarrier();
};

class InteractiveBox : GizmoBase
{};

// a selectable GUI overlay/3D object in scene for editor
class Gizmos
{
public:
    static InteractiveBox* DrawInteractiveBox();
    static GizmoBase* DrawMesh(Mesh& mesh, int submeshIndex, ObjPtr<Shader2> shader, const glm::mat4& modelMatrix);
    static GizmoBase* DrawMesh(Mesh& mesh, int submeshIndex, Material* shader, const glm::mat4& modelMatrix);
    static GizmoBase* DrawLight(const glm::vec3& position);
    static GizmoBase* DrawCamera(const glm::vec3& position);
    static void DispatchAllDiszmos(Gfx::CommandBuffer& cmd, Gfx::ShaderResource* perScene);

    int GetSize() { return gizmos.size(); }

    static bool RayVsAABB(const Ray& r, const AABB& aabb, float& t);
    static void PickGizmos(const Ray& ray, DynamicArray<GameObject*>& result);
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
    DynamicArray<std::unique_ptr<GizmoBase>> gizmos;
};
