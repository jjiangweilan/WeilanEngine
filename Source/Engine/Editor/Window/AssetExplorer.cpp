#include "AssetExplorer.hpp"
#include "Core/Model.hpp"
#include "Core/AssetDatabase/AssetDatabase.hpp"
#include "Core/AssetObject.hpp"
#include "Core/Graphics/Material.hpp"
#include "../EditorRegister.hpp"
#include "ThirdParty/imgui/imgui.h"
#include "../Extension/CustomExplore/ModelExplorer.hpp"
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
        for (auto const& dir_entry : std::filesystem::directory_iterator(path))
        {
            auto dirPath = dir_entry.path();
            if (std::filesystem::is_directory(dir_entry))
            {
                if (ImGui::TreeNode(dirPath.filename().string().c_str()))
                {
                    ShowDirectory(dir_entry);
                    ImGui::TreePop();
                }

                if (ImGui::BeginPopupContextWindow("ContextWindow"))
                {
                    CreateNewAssetMenuItem<Material>("Material", path);
                    ImGui::EndPopup();
                }
            }
            else if(dirPath.extension() != ".meta")
            {
                RefPtr<AssetObject> assetObject = AssetDatabase::Instance()->Load(dirPath);

                if (assetObject != nullptr)
                {
                    // if we have a custom exploerer then use it. Otherwise, we use the default exploerer
                    auto customExplorer = EditorRegister::Instance()->GetCustomExplorer(assetObject);
                    if (customExplorer != nullptr)
                    {
                        customExplorer->Tick(editorContext, dirPath);
                    }
                    else
                    {
                        auto assetObjectName = assetObject->GetName();
                        if (ImGui::Button(dirPath.filename().string().c_str()))
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
        ShowDirectory("Assets");
        ImGui::End();
    }
}
