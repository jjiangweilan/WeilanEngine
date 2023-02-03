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
    const char* GetDisplayWindowName() override;
    ImGuiWindowFlags GetWindowFlags() override;

private:
    struct LockedContext
    {
        EditorContext context;
        bool isLocked = false;
    } lockedContext;

    void LockContext()
    {
        lockedContext.context = *editorContext;
        lockedContext.isLocked = true;
    }

    EditorContext* GetContext()
    {
        if (lockedContext.isLocked)
        {
            return &lockedContext.context;
        }
        else
            return editorContext.Get();
    }

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
