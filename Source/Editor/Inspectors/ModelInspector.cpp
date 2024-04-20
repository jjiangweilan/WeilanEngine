#include "../EditorState.hpp"
#include "Core/Model.hpp"
#include "GameEditor.hpp"
#include "Inspector.hpp"
#include "ThirdParty/imgui/imgui_internal.h"
#include "WeilanEngine.hpp"

namespace Editor
{
class ModelInspector : public Inspector<Model>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        // object information
        ImGui::Text("%s", target->GetName().c_str());

        ImGui::Text("Meshes");
        ImGui::Indent();
        for (auto& mesh : target->GetMeshes())
        {
            ImGui::Button(mesh->GetName().c_str());

            if (ImGui::BeginDragDropSource())
            {
                Mesh* meshPtr = mesh.get();
                ImGui::SetDragDropPayload("object", &meshPtr, sizeof(void*));
                ImGui::Text("%s", mesh->GetName().c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                EditorState::selectedObject = mesh->GetSRef();
            }
        }
        ImGui::Unindent();

        ImGui::Text("Materials");
        ImGui::Indent();
        size_t pushID = 0;
        for (auto& material : target->GetMaterials())
        {
            ImGui::PushID(pushID++);

            Material* mat = material.get();
            ImGui::Button(mat->GetName().c_str());

            if (ImGui::BeginDragDropSource())
            {
                Material* matPtr = material.get();
                ImGui::SetDragDropPayload("object", &matPtr, sizeof(void*));
                ImGui::Text("%s", material->GetName().c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                EditorState::selectedObject = material->GetSRef();
            }

            ImGui::SameLine();
            if (ImGui::Button("copy"))
            {
                auto copy = material->Clone();
                auto& db = editor.GetEngine()->assetDatabase;
                db->SaveAsset(std::move(copy), material->GetName());
            }

            ImGui::PopID();
        }
        ImGui::Unindent();

        ImGui::Text("Textures");
        ImGui::Indent();
        for (auto& texture : target->GetTextures())
        {
            Texture* tex = texture.get();
            auto width = tex->GetDescription().img.width;
            auto height = tex->GetDescription().img.height;
            auto size = ResizeKeepRatio(width, height, 100, 100);
            ImGui::Text("%s", tex->GetName().c_str());
            ImGui::SameLine();
            ImGui::Image(&tex->GetGfxImage()->GetDefaultImageView(), {size.x, size.y});

            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                Texture* texPtr = texture.get();
                ImGui::SetDragDropPayload("object", &texPtr, sizeof(void*));
                ImGui::Text("%s", texture->GetName().c_str());
                ImGui::EndDragDropSource();
            }

            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                EditorState::selectedObject = texture->GetSRef();
            }
        }
        ImGui::Unindent();
    }

private:
    static const char _register;

    glm::vec2 ResizeKeepRatio(float width, float height, float contentWidth, float contentHeight)
    {
        float imageWidth = width;
        float imageHeight = height;

        // shrink width
        if (imageWidth > contentWidth)
        {
            float ratio = contentWidth / (float)imageWidth;
            imageWidth = contentWidth;
            imageHeight *= ratio;
        }

        if (imageHeight > contentHeight)
        {
            float ratio = contentHeight / (float)imageHeight;
            imageHeight = contentHeight;
            imageWidth *= ratio;
        }

        return {imageWidth, imageHeight};
    }
};

const char ModelInspector::_register = InspectorRegistry::Register<ModelInspector, Model>();

} // namespace Editor
