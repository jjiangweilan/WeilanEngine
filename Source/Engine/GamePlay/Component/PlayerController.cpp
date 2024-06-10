#include "PlayerController.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/GameObject.hpp"
#include "Core/Time.hpp"
#include "Gameplay/Input.hpp"
#include <spdlog/spdlog.h>

DEFINE_OBJECT(PlayerController, "A14D66B4-47AB-4703-BEAA-06EBD285034F");

PlayerController::PlayerController() : Component(nullptr) {}

PlayerController::PlayerController(GameObject* gameObject) : Component(gameObject) {}

const std::string& PlayerController::GetName()
{
    static std::string name = "PlayerController";
    return name;
}

std::unique_ptr<Component> PlayerController::Clone(GameObject& owner)
{
    std::unique_ptr<PlayerController> clone = std::make_unique<PlayerController>(&owner);
    clone->SetCamera(target);
    clone->movementSpeed = movementSpeed;
    clone->cameraOffset = cameraOffset;

    return clone;
}
void PlayerController::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("target", target);
    s->Serialize("cameraOffset", cameraOffset);
    s->Serialize("movementSpeed", movementSpeed);
}
void PlayerController::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("target", target);
    s->Deserialize("cameraOffset", cameraOffset);
    s->Deserialize("movementSpeed", movementSpeed);
}

void PlayerController::Tick()
{
    if (target)
    {
        auto targetGO = target->GetGameObject();
        if (targetGO == GetGameObject())
            return;

        // set movement
        auto characterPos = GetGameObject()->GetPosition();
        float x, y;
        Input::GetSingleton().GetMovement(x, y);
        x *= movementSpeed;
        y *= movementSpeed;
        auto forward = targetGO->GetForward();

        characterPos += glm::vec3(x * Time::DeltaTime(), 0, y * Time::DeltaTime());
        GetGameObject()->SetPosition(characterPos);

        // set camera rotation;
        glm::vec3 cameraPos = targetGO->GetPosition();
        auto lookAtQuat = glm::quatLookAtLH(glm::normalize(characterPos - cameraPos), glm::vec3(0, 1, 0));
        targetGO->SetRotation(lookAtQuat);

        // set camera position;
        targetGO->SetPosition(GetGameObject()->GetPosition() + cameraOffset);
    }
}
