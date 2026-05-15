#pragma once
#include "Editor/Window.hpp"
#include <memory>

class GrassSurface;

namespace Editor
{
class GrassSurfacePaintTool;

class GrassSurfacePaintWindow : public Window
{
    GrassSurfacePaintWindow() {}
    static bool _editorWindowRegistered;
    friend class WindowRegistery;
    friend class GameEditor;

public:
    bool Tick() override;
    void OnClose() override;

    void SetTargetGrassSurface(GrassSurface* gs);
    GrassSurfacePaintTool* GetTool() const { return tool.get(); }
    bool IsToolActive() const { return isActive; }
    void SetToolActive(bool active) { isActive = active; }

private:
    std::unique_ptr<GrassSurfacePaintTool> tool;
    bool isActive = false;
};
} // namespace Editor