#pragma once
#include "./GizmoBase.hpp"
#include "./GizmoHandle.hpp"
#include "./GizmoRenderer.hpp"
#include "Engine/Library/Math.hpp"
#include <unordered_map>

class GizmoManager
{
public:
    GizmoManager();
    using GizmoList = std::list<GizmoState>;

    void DrawMesh(
        GizmoHandle& handle, Mesh* mesh, int submeshIndex, ObjPtr<Shader> shader, const glm::mat4& modelMatrix
    );
    void DrawMesh(GizmoHandle& handle, Mesh* mesh, int submeshIndex, Material* shader, const glm::mat4& modelMatrix);

    template <class GizmoType, class... Params>
        requires std::is_base_of_v<GizmoBase, GizmoType>
    void Draw(GizmoHandle& handle, Params&&... params)
    {
        if (!ValidateGizmoHandle(handle))
        {
            return;
        }

        if (handle.selfNode->ptr == nullptr)
        {
            handle.selfNode->ptr = std::make_unique<GizmoType>();
            handle.selfNode->ptr->Setup(ediotrContext);
        }

        auto* gizmo = static_cast<GizmoType*>(handle.selfNode->ptr.get());

        gizmo->ProcessUserInput(std::forward<Params>(params)...);
        anyActiveGizmo = anyActiveGizmo || gizmo->IsActive();
    }

    bool AnyGizmoActive() { return anyActiveGizmo; }
    void ResetState() { anyActiveGizmo = false; }

    const GizmoList& GetActiveGizmos() { return *activeGizmos; }

    void ClearInactiveGizmos();
    void Render(Camera* camera, Gfx::ShaderResource* perScene, Gfx::CommandBuffer& cmd)
    {
        renderer.SetupDraw(camera, perScene);
        renderer.Draw(*this, cmd);
    }
    void SetEditorContext(EditorContext* context) { ediotrContext = context; }

private:
    const int maximumActiveGizmos = 64;
    EditorContext* ediotrContext = nullptr;
    GizmoList gizmoList0;
    GizmoList gizmoList1;
    GizmoList* activeGizmos = &gizmoList0;
    GizmoList* inactiveGizmos = &gizmoList1;
    GizmoRenderer renderer;
    bool anyActiveGizmo = false;

    bool ValidateGizmoHandle(GizmoHandle& handle);

    void GetHandleID(uint32_t& outID, uint32_t& outGeneration);
};
