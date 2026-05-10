#pragma once
#include "Engine/Library/Math/Geometry/Geometry.hpp"
#include "Engine/Library/Math.hpp"

namespace Gfx
{
class CommandBuffer;
}

class GizmoManager;
class EditorContext;

namespace Editor
{
class SceneEditor;

struct SceneEditorToolContext
{
    SceneEditor* editor;
    glm::vec2 screenUV;
    Ray worldRay;
    bool sceneViewHovered;
    ::GizmoManager* gizmoManager;
    ::EditorContext* editorContext;
};

class SceneEditorTool
{
public:
    virtual ~SceneEditorTool() = default;

    /**
     * @brief Called every frame while the tool is active.
     * @param ctx Tool context with ray, UV, hovered state, etc.
     * @return true if the tool consumed input (prevents default picking/gizmo).
     */
    virtual bool Tick(const SceneEditorToolContext& ctx) = 0;

    /**
     * @brief Called during the scene render pass for overlay drawing.
     */
    virtual void OnDraw(Gfx::CommandBuffer& cmd) {}

    virtual void OnActivate() {}
    virtual void OnDeactivate() {}
};
} // namespace Editor
