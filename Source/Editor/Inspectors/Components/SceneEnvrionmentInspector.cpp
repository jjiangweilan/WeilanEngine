#include "../../EditorState.hpp"
#include "../Inspector.hpp"
#include "Core/Component/SceneEnvironment.hpp"
#include "Core/Texture.hpp"

namespace Editor
{
class SceneEnvironmentInspector : public Inspector<SceneEnvironment>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        auto diffuseCube = target->GetDiffuseCube();
        auto specularCube = target->GetSpecularCube();

        ImGui::Text("DiffuseCube");

        if (diffuseCube)
        {
            // auto size = ResizeKeepRatio(diffuseCube);
            // ImGui::Image(&diffuseCube->GetGfxImage()->GetDefaultImageView(), {size.x, size.y});
            if (ImGui::Button("editor renderer doesn't support rendering cubemap yet, click me to see the texture##Diffuse"))
            {
                EditorState::selectedObject = diffuseCube;
            }
        }
        else
            ImGui::Button("empty");

        if (ImGui::BeginDragDropTarget())
        {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("object");
            if (payload && payload->IsDelivery())
            {
                Object& obj = **(Object**)payload->Data;
                if (typeid(obj) == typeid(Texture))
                {
                    Texture* tex = static_cast<Texture*>(&obj);

                    target->SetDiffuseCube(tex);
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::Text("SpecularCube");
        if (specularCube)
        {
            if (ImGui::Button("editor renderer doesn't support rendering cubemap yet, click me to see the texture##Specular"))
            {
                EditorState::selectedObject = specularCube;
            }
        }
        else
            ImGui::Button("empty");

        if (ImGui::BeginDragDropTarget())
        {
            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("object");
            if (payload && payload->IsDelivery())
            {
                Object& obj = **(Object**)payload->Data;
                if (typeid(obj) == typeid(Texture))
                {
                    Texture* tex = static_cast<Texture*>(&obj);

                    target->SetSpecularCube(tex);
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

private:
    glm::vec2 ResizeKeepRatio(Texture* tex)
    {
        auto width = tex->GetDescription().img.width;
        auto height = tex->GetDescription().img.height;
        auto contentMax = ImGui::GetWindowContentRegionMax();
        auto contentMin = ImGui::GetWindowContentRegionMin();
        auto contentWidth = contentMax.x - contentMin.y;
        auto contentHeight = contentMax.y - contentMin.y;

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
    static const char _register;
};

const char SceneEnvironmentInspector::_register =
    InspectorRegistry::Register<SceneEnvironmentInspector, SceneEnvironment>();

} // namespace Editor
