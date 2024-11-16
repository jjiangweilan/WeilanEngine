#include "../EditorState.hpp"
#include "Core/Model.hpp"
#include "EditorGUI.hpp"
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

            GUI::DragDropSource(mesh->GetName().c_str(), mesh.get());

            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                EditorState::SelectObject(mesh->GetSRef());
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

            GUI::DragDropSource(material->GetName().c_str(), material.get());

            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                EditorState::SelectObject(material->GetSRef());
            }

            ImGui::SameLine();
            if (ImGui::Button("copy"))
            {
                auto copy = material->Clone();
                auto& db = editor.GetEngine()->assetDatabase;
                std::filesystem::path savePath = material->GetName();
                if (!savePath.has_extension() || savePath.extension() != ".mat")
                {
                    savePath = savePath.filename().replace_extension(".mat");
                }
                db->SaveAsset(std::move(copy), savePath);
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

            GUI::DragDropSource(texture->GetName().c_str(), texture.get());

            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                EditorState::SelectObject(texture->GetSRef());
            }
        }
        ImGui::Unindent();

        ImGui::Text("Animations");
        ImGui::Indent();
        for (auto& animation : target->GetAnimations())
        {
            Animation* anim = animation.get();

            if(ImGui::Button(anim->GetName().c_str()))
            {
                EditorState::SelectObject(anim->GetSRef());
            }
            GUI::DragDropSource(anim->GetName().c_str(), anim);
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
