#include "../Inspector.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/GameObject.hpp"
#include "EditorGUI.hpp"
#include "GamePlay/Component/PlayerController.hpp"
#include "Rendering/Graphics.hpp"

namespace Editor
{
class PlayerControllerInspector : public Inspector<PlayerController>
{
public:
    void DrawInspector(GameEditor& editor) override
    {
        Inspector<PlayerController>::DrawInspector(editor);

        Camera* camera = target->GetCamera();
        if (GUI::ObjectField("camera", camera))
        {
            target->SetCamera(camera);
        }

        ImGui::DragFloat("movementSpeed", &target->movementSpeed);
        ImGui::DragFloat("rotateSpeed", &target->rotateSpeed);
        ImGui::DragFloat("cameraOffset", &target->cameraDistance);
        ImGui::DragFloat("jumpForce", &target->jumpImpulse);

        auto go = target->GetGameObject();
        float radius = target->GetCharacterCapsuleShapeRadius();
        float height = target->GetCharacterCapsuleShapeHeight();

        bool radiusOrHeightChanged = false;
        radiusOrHeightChanged |= ImGui::DragFloat("height", &height);
        radiusOrHeightChanged |= ImGui::DragFloat("radius", &radius);
        if (radiusOrHeightChanged)
        {
            target->SetCharacterCapsuleShape(height, radius);
        }

        Graphics::DrawCapsule(
            height,
            radius,
            go->GetPosition() + glm::vec3{0, height + radius, 0},
            go->GetRotation(),
            go->GetScale()
        );
    }

private:
    static const char _register;
};

const char PlayerControllerInspector::_register =
    InspectorRegistry::Register<PlayerControllerInspector, PlayerController>();

} // namespace Editor
