#pragma once
#include "Editor/Window.hpp"

namespace Editor
{
class GPUDrivenManagerDebugWindow : public Window
{
    DECLARE_EDITOR_WINDOW(GPUDrivenManagerDebugWindow)
public:
    bool Tick() override;
};
} // namespace Editor
