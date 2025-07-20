#include "../Inspector.hpp"
#include "Core/Component/AnimationPlayer.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/GameObject.hpp"
#include "EditorGUI.hpp"
#include "Gameplay/Component/PlayerController.hpp"
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
        auto obj = target->rotationRoot.Get();
        if (GUI::ObjectField("rotation root", obj))
        {
            target->rotationRoot = obj;
        }

        AnimationPlayer* animationPlayer = target->GetRootMotionAnimationPlayer();
        if (GUI::ObjectField("root motion animation player", animationPlayer))
        {
            target->SetRootMotionAnimationPlayer(animationPlayer);
        }

        ImGui::SeparatorText("Player");
        ImGui::DragFloat("Movement Speed", &target->movementSpeed);

        ImGui::SeparatorText("Camera");
        ImGui::DragFloat("Rotate Speed", &target->rotateSpeed);
        ImGui::DragFloat("Elasticity", &target->cameraElasticity);
        ImGui::DragFloat("Min Elasticity", &target->cameraMinElasticity);
        ImGui::DragFloat("Offset", &target->cameraOffset);
        ImGui::Text("Phi %f", target->cameraPhi);
        ImGui::Text("Theta %f", target->cameraTheta);

        ImGui::SeparatorText("Animation");
        ImGui::DragFloat("blendFactorScale", &target->blendFactorScale);

        auto go = target->GetGameObject();
        float radius = target->GetCharacterCapsuleShapeRadius();
        float halfHeight = target->GetCharacterCapsuleShapeHalfHeight();

        bool radiusOrHeightChanged = false;
        radiusOrHeightChanged |= ImGui::DragFloat("halfHeight", &halfHeight);
        radiusOrHeightChanged |= ImGui::DragFloat("radius", &radius);
        if (radiusOrHeightChanged)
        {
            target->SetCharacterCapsuleShape(halfHeight, radius);
        }

        ImGui::Checkbox("Debug Draw", &debugDraw);
        if (debugDraw)
        {
            Graphics::DrawCapsule(
                halfHeight,
                radius,
                go->GetPosition() + glm::vec3{0, halfHeight + radius, 0},
                go->GetRotation(),
                go->GetScale()
            );
        }
    }

private:
    static const char _register;
    bool debugDraw = false;
};

const char PlayerControllerInspector::_register =
    InspectorRegistry::Register<PlayerControllerInspector, PlayerController>();

} // namespace Editor
