#include "../../EditorState.hpp"
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

        ImGui::NewLine();
        GUI::SeparatorTextLabeled("Environment");
        Texture* skybox = target->GetDiffuseEnv().Get();
        Texture* specularEnv = target->GetSpecularEnv().Get();
        if (GUI::ObjectField("Diffuse Environment Map", skybox))
        {
            if (skybox)
                target->SetDiffuseEnv(skybox);
        }

        if (GUI::ObjectField("Specular Environment Map", specularEnv))
        {
            if (specularEnv)
                target->SetSpecularEnv(specularEnv);
        }

        ImGui::NewLine();
        if (GUI::ButtonSimple("Set as main camera"))
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
