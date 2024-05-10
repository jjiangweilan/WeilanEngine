#include "Core/Component/Camera.hpp"
#include "PlayerController.hpp"

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
    std::unique_ptr<PlayerController> clone = std::make_unique<PlayerController>();
    clone->SetCamera(target);

    return clone;
}
void PlayerController::Serialize(Serializer* s) const
{
    s->Serialize("target", target);
}
void PlayerController::Deserialize(Serializer* s)
{
    s->Deserialize("target", target);
}
