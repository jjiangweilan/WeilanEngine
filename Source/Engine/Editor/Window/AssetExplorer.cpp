#include "AssetExplorer.hpp"
#include "Core/Model.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Core/AssetObject.hpp"
#include "Core/Graphics/Material.hpp"
#include "../EditorRegister.hpp"
#include "../imgui/imgui.h"
#include "../Extension/CustomExplore/ModelExplorer.hpp"
#include "Core/Global/Global.hpp"
#include <spdlog/spdlog.h>

namespace Engine::Editor
{
    AssetExplorer::AssetExplorer(RefPtr<EditorContext> editorContext) : editorContext(editorContext)
    {
        EditorRegister::Instance()->RegisterCustomExplorer<Model, ModelExplorer>();
    }

    template<class T>
    void CreateNewAssetMenuItem(const std::string& assetName, const std::filesystem::path& path)
    {
        if(ImGui::MenuItem((std::string("Create ") + assetName).c_str()))
        {
            auto theAsset = MakeUnique<T>();

            uint32_t i = 0;
            std::filesystem::path matPath;
            std::string actualName;
            do
            {
                matPath = path;
                if (i == 0)
                {
                    actualName = assetName;
                    matPath.append(actualName + ".mat");
                }
                else
                {
                    actualName = assetName + std::to_string(i);
                    matPath.append(actualName + ".mat");
                }
                i++;
            } while (std::filesystem::exists(matPath));
            theAsset->SetName(actualName);
            AssetDatabase::Instance()->Save(std::move(theAsset), matPath);
        }
    }

    void AssetExplorer::ShowDirectory(const std::filesystem::path& path)
    {
        if (std::filesystem::is_directory(path))
        {
            if (ImGui::TreeNode(path.filename().string().c_str()))
            {
                for (auto const& dir_entry : std::filesystem::directory_iterator(path)) 
                {
                    ShowDirectory(dir_entry);
                }

                if (ImGui::BeginPopupContextWindow("ContextWindow"))
                {
                    CreateNewAssetMenuItem<Material>("Material", path);
                    ImGui::EndPopup();
                }
                ImGui::TreePop();
            }
        }
        else
        {
            if (path.extension() != ".meta")
            {
                RefPtr<AssetObject> assetObject = AssetDatabase::Instance()->Load(path);

                if (assetObject != nullptr)
                {
                    // if we have a custom exploerer then use it. Otherwise, we use the default exploerer
                    auto customExplorer = EditorRegister::Instance()->GetCustomExplorer(assetObject);
                    if (customExplorer != nullptr)
                    {
                        customExplorer->Tick(editorContext, path);
                    }
                    else
                    {
                        auto assetObjectName = assetObject->GetName();
                        if (ImGui::Button(assetObjectName.c_str()))
                        {
                            editorContext->currentSelected = assetObject;
                        }
                        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
                        {
                            AssetObject* payload = assetObject.Get();
                            ImGui::SetDragDropPayload("GameEditorDNDPayload", &payload, sizeof(assetObject.Get()));
                            ImGui::Text("%s", assetObjectName.c_str());
                            ImGui::EndDragDropSource();
                        }
                    }
                }
            }
        }
    }

    void AssetExplorer::Tick()
    {
        ImGui::Begin("Asset");
        ShowDirectory(Global::Instance()->projectConfig.GetProjectRootPath() / "Assets");
        ImGui::End();
    }

    void AssetExplorer::Refresh()
    {

    }
}
