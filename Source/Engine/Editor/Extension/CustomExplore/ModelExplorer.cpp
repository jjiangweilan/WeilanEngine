#include "ModelExplorer.hpp"
#include "Core/Model.hpp"
#include "ThirdParty/imgui/imgui.h"
namespace Engine::Editor
{
void ModelExplorer::Tick(RefPtr<EditorContext> editorContext, const std::filesystem::path& path)
{
    RefPtr<Model> model = target;
    if (ImGui::TreeNode(path.filename().string().c_str()))
    {
        for (const auto& name : model->GetMeshNames())
        {
            if (ImGui::Button(name.c_str()))
            {
                editorContext->currentSelected = (AssetObject*)target.Get();
            }

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
            {
                Mesh* data = model->GetMesh(name).Get();
                ImGui::SetDragDropPayload("GameEditorDNDPayload", &data, sizeof(data));
                ImGui::Text("%s", name.c_str());
                ImGui::EndDragDropSource();
            }
        }
        ImGui::TreePop();
    }
}
} // namespace Engine::Editor
