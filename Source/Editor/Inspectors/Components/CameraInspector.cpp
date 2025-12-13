#include "Editor/EditorState.hpp"
#include "../Inspector.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Scene/SceneManager.hpp"
#include "DragDropIDs.hpp"
#include "EditorGUI.hpp"
#include "Rendering/Graphics.hpp"
namespace Editor
{
class CameraInspector : public Inspector<Camera>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<Camera>::DrawInspector(editor);

        // Projection controls
        ImGui::NewLine();
        EditorGUI::SeparatorTextLabeled("Projection");
        EditorGUI::ObjectPropertyEnum(
            "Projection Mode",
            std::vector<std::string>{"Perspective", "Orthographic"},
            *target,
            &Camera::GetProjectionMode,
            &Camera::SetProjectionMode
        );

        if (target->GetProjectionMode() == Camera::ProjectionMode::Perspective)
        {
            float fovDegrees = glm::degrees(target->GetFoV());
            if (EditorGUI::DragFloat("Field of View", &fovDegrees, 0.1f, 1.0f, 179.0f))
            {
                target->SetFoV(glm::radians(fovDegrees));
            }
        }
        else if (target->GetProjectionMode() == Camera::ProjectionMode::Orthographic)
        {
            float orthoSize = target->GetOrthographicSize();
            if (EditorGUI::DragFloat("Orthographic Size", &orthoSize, 0.05f, 0.0001f))
            {
                target->SetOrthographicSize(orthoSize);
            }
        }

        ImGui::NewLine();
        EditorGUI::SeparatorTextLabeled("Environment");
        Texture* skybox = target->GetDiffuseEnv().Get();
        Texture* specularEnv = target->GetSpecularEnv().Get();
        if (EditorGUI::ObjectField("Diffuse Environment Map", skybox))
        {
            if (skybox)
                target->SetDiffuseEnv(skybox);
        }

        if (EditorGUI::ObjectField("Specular Environment Map", specularEnv))
        {
            if (specularEnv)
                target->SetSpecularEnv(specularEnv);
        }

        ImGui::NewLine();
        if (EditorGUI::ButtonSimple("Set as main camera"))
        {
            if (auto scene = SceneManager::GetActiveScene())
            {
                scene->SetMainCamera(target);
            }
        }

        Graphics::DrawFrustum(target->GetAndUpdateProjectionMatrix() * target->GetViewMatrix());
    }

private:
    static const char _register;
};

const char CameraInspector::_register = InspectorRegistry::Register<CameraInspector, Camera>();

} // namespace Editor
