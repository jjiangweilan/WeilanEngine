#include "InspectorWindow.hpp"
#include "../EditorRegister.hpp"
#include "Core/Component/Components.hpp"
#include "Core/Component/MeshRenderer.hpp"
#include "Core/Component/Transform.hpp"
#include "Core/Model.hpp"
#include "GfxDriver/ShaderLoader.hpp"
#include "ThirdParty/imgui/imgui.h"
namespace Engine::Editor
{
InspectorWindow::InspectorWindow(RefPtr<EditorContext> editorContext) : editorContext(editorContext) {}

const char* InspectorWindow::GetWindowName()
{
    auto activeObject = editorContext->currentSelected;
    windowTitle = "Inspector";
    windowTitle += activeObject == nullptr ? "" : (" - " + activeObject->GetName());
    return windowTitle.c_str();
}

ImGuiWindowFlags_ InspectorWindow::GetWindowFlags()
{
    auto activeObject = editorContext->currentSelected;
    auto asAssetObject = dynamic_cast<AssetObject*>(activeObject.Get());
    if (asAssetObject)
    {
        return ImGuiWindowFlags_MenuBar;
    }
    return ImGuiWindowFlags_None;
}

void InspectorWindow::Tick()
{
    auto activeObject = editorContext->currentSelected;
    auto asAssetObject = dynamic_cast<AssetObject*>(activeObject.Get());

    if (asAssetObject)
    {
        ImGui::BeginMenuBar();
        if (ImGui::MenuItem("Object"))
            type = InspectorType::Object;
        if (ImGui::MenuItem("Import"))
            type = InspectorType::Import;
        ImGui::EndMenuBar();
    }
    else
        type = InspectorType::Object;

    if (activeObject)
    {
        switch (type)
        {
            case InspectorType::Object:
                {
                    auto inspector = EditorRegister::Instance()->GetInspector(activeObject);
                    if (inspector != nullptr)
                    {
                        inspector->Tick(editorContext);
                    }
                    break;
                }
            case InspectorType::Import:
                {
                    if (isFocused != ImGui::IsWindowFocused())
                    {
                        isFocused = ImGui::IsWindowFocused();
                        importConfig = AssetDatabase::Instance()->GetImporterConfig(asAssetObject->GetUUID(),
                                                                                    asAssetObject->GetTypeInfo());
                    }

                    for (auto c = importConfig.begin(); c != importConfig.end(); ++c)
                    {
                        if (c.value().is_boolean())
                        {
                            bool t = c.value();
                            ImGui::Checkbox(c.key().c_str(), &t);
                            *c = t;
                        }
                        else
                        {
                            ImGui::Text("%s", c.key().c_str());
                        }
                    }

                    if (ImGui::Button("Apply"))
                    {
                        AssetDatabase::Instance()->Reimport(asAssetObject->GetUUID(), importConfig);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Reset"))
                    {
                        importConfig = AssetDatabase::Instance()->GetImporterConfig(asAssetObject->GetUUID(),
                                                                                    asAssetObject->GetTypeInfo());
                    }
                    break;
                }
        }
    }
}
} // namespace Engine::Editor
