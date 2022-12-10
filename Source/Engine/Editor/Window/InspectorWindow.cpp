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
        ImGuiWindowFlags_ windowFlags = ImGuiWindowFlags_None;

        auto asAssetObject = dynamic_cast<AssetObject*>(activeObject.Get());
        if (asAssetObject)
        {
            windowFlags = ImGuiWindowFlags_MenuBar;
        }
        ImGui::Begin(windowTitle.c_str(), nullptr, windowFlags);

        if (asAssetObject)
        {
            ImGui::BeginMenuBar();
            if (ImGui::MenuItem("Object")) type = InspectorType::Object;
            if (ImGui::MenuItem("Import")) type = InspectorType::Import;
            ImGui::EndMenuBar();
        }
        else type = InspectorType::Object;

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
                        importConfig = AssetDatabase::Instance()->GetImporterConfig(asAssetObject->GetUUID(), asAssetObject->GetTypeInfo());
                    }

                    for(auto c = importConfig.begin(); c != importConfig.end(); ++c)
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
                        importConfig = AssetDatabase::Instance()->GetImporterConfig(asAssetObject->GetUUID(), asAssetObject->GetTypeInfo());
                    }
                    break;
                }
            }
        }
        ImGui::End();
    }
}
