#pragma once

#include "../EditorContext.hpp"
#include "Core/Graphics/Material.hpp"
#include "Libs/Ptr.hpp"
namespace Engine::Editor
{
class InspectorWindow
{
public:
    InspectorWindow(RefPtr<EditorContext> editorContext);
    void Tick();

private:
    RefPtr<EditorContext> editorContext;
    enum class InspectorType
    {
        Object,
        Import
    } type;
    bool isFocused = false;
    nlohmann::json importConfig;
};
} // namespace Engine::Editor
