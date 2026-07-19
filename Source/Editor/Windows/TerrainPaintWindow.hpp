#pragma once

#include "Editor/Window.hpp"
#include <memory>

class Terrain;

namespace Editor
{
class TerrainPaintTool;

class TerrainPaintWindow final : public Window
{
    TerrainPaintWindow() = default;
    static bool _editorWindowRegistered;
    friend class WindowRegistery;
    friend class GameEditor;

public:
    bool Tick() override;
    void OnClose() override;
    void SetTargetTerrain(Terrain* terrain);
    TerrainPaintTool* GetTool() const { return tool.get(); }
    void SetToolActive(bool active) { isActive = active; }

private:
    std::unique_ptr<TerrainPaintTool> tool;
    bool isActive = false;
};
} // namespace Editor
