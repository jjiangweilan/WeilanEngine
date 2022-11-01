#include "InspectorWindow.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Component/Transform.hpp"
#include "Core/Component/Components.hpp"
#include "Core/Model.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "../EditorRegister.hpp"
#include "GfxDriver/ShaderLoader.hpp"
namespace Engine::Editor
{
    InspectorWindow::InspectorWindow(RefPtr<EditorContext> editorContext) : editorContext(editorContext)
    {

    }

    void InspectorWindow::Tick()
    {
        auto activeObject = editorContext->currentSelected;
        std::string windowTitle = "Inspector";
        windowTitle += activeObject == nullptr ? "" : (" - " + activeObject->GetName());
        windowTitle += "###Inspector";
        ImGui::Begin(windowTitle.c_str());

        if (activeObject)
        {
            auto inspector = EditorRegister::Instance()->GetInspector(activeObject);
            if (inspector != nullptr)
            {
                inspector->Tick(editorContext);
            }
        }
        ImGui::End();
    }
}
