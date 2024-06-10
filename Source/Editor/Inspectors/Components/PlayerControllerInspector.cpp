#include "../Inspector.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/GameObject.hpp"
#include "EditorGUI/ObjectField.hpp"
#include "GamePlay/Component/PlayerController.hpp"

namespace Editor
{
class PlayerControllerInspector : public Inspector<PlayerController>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<PlayerController>::DrawInspector(editor);

        Camera* camera = target->GetCamera();
        if (GUI::ObjectField(fmt::format("{}", camera ? camera->GetGameObject()->GetName().c_str() : "null"), camera))
        {
            target->SetCamera(camera);
        }

        ImGui::DragFloat("movementSpeed", &target->movementSpeed);
        ImGui::DragFloat("rotateSpeed", &target->rotateSpeed);
        ImGui::DragFloat("cameraOffset", &target->cameraDistance);
        ImGui::DragFloat("jumpForce", &target->jumpImpulse);
    }

private:
    static const char _register;
};

const char PlayerControllerInspector::_register =
    InspectorRegistry::Register<PlayerControllerInspector, PlayerController>();

} // namespace Editor
