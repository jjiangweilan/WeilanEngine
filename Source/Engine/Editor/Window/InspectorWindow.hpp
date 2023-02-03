#pragma once

#include "../EditorContext.hpp"
#include "Core/Graphics/Material.hpp"
#include "EditorWindow.hpp"
#include "Libs/Ptr.hpp"
namespace Engine::Editor
{
class InspectorWindow : public EditorWindow
{
public:
    InspectorWindow(RefPtr<EditorContext> editorContext);
    void Tick() override;

    // configuration function
    const char* GetWindowName() override;
    ImGuiWindowFlags_ GetWindowFlags() override;

private:
    RefPtr<EditorContext> editorContext;
    enum class InspectorType
    {
        Object,
        Import
    } type;
    bool isFocused = false;
    nlohmann::json importConfig;
    std::string windowTitle;
};
} // namespace Engine::Editor
