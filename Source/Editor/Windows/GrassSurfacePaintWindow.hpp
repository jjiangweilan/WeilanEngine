#pragma once
#include "Editor/Window.hpp"
#include <memory>

namespace Editor
{
class GrassSurfacePaintTool;

class GrassSurfacePaintWindow : public Window
{
    DECLARE_EDITOR_WINDOW(GrassSurfacePaintWindow)

public:
    bool Tick() override;

private:
    std::unique_ptr<GrassSurfacePaintTool> tool;
    bool isActive = false;
};
} // namespace Editor
