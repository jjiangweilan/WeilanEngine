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
class Camera;

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
     * @brief Tick gizmo for editor interaction
     */
    virtual void Tick() {}

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

    /**
     * @brief Used to prepare drawing
     *
     * @param perScene the scene shader resource of currently rendering scene
     */
    void SetupDraw(EditorContext* editorContext, Camera* camera, Gfx::ShaderResource* perScene)
    {
        this->camera = camera;
        this->perScene = perScene;
    }

    GameObject* GetCarrier() { return carrier; }

    bool m_IsActive = false;

protected:
    Camera* camera;
    Gfx::ShaderResource* perScene;

private:
    GameObject* carrier;
    static GameObject*& GetActiveCarrier();
};
