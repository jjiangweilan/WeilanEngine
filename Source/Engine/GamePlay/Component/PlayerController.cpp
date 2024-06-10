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

    return clone;
}
void PlayerController::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("target", target);
    s->Serialize("movementSpeed", movementSpeed);
}
void PlayerController::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("target", target);
    s->Deserialize("movementSpeed", movementSpeed);
}

void PlayerController::Tick()
{
    if (target)
    {
        auto cameraGO = target->GetGameObject();
        if (cameraGO == GetGameObject())
            return;

        // set movement
        auto characterPos = GetGameObject()->GetPosition();
        float mx, my;
        Input::GetSingleton().GetMovement(mx, my);
        mx *= movementSpeed * Time::DeltaTime();
        my *= movementSpeed * Time::DeltaTime();

        auto forward = cameraGO->GetForward();
        auto right = cameraGO->GetRight();

        // mute y
        forward.y = 0;
        right.y = 0;
        forward = glm::normalize(forward);
        right = glm::normalize(right);

        forward *= my;
        right *= mx;

        characterPos += (forward + right);
        GetGameObject()->SetPosition(characterPos);

        // set camera lookat (camera position)
        float lx, ly;
        Input::GetSingleton().GetLookAround(lx, ly);
        SetCameraSphericalPos(-lx, -ly);

        // set camera lookat character
        glm::vec3 cameraPos = cameraGO->GetPosition();
        auto lookAtQuat = glm::quatLookAtLH(glm::normalize(characterPos - cameraPos), glm::vec3(0, 1, 0));
        cameraGO->SetRotation(lookAtQuat);
    }
}
void PlayerController::Awake()
{
    auto targetGO = target->GetGameObject();
    if (targetGO == GetGameObject())
        return;

    // set camera's initial position
    SetCameraSphericalPos(0, 0);

    // set camera's initial rotation
    auto characterPos = GetGameObject()->GetPosition();
    glm::vec3 cameraPos = targetGO->GetPosition();
    auto lookAtQuat = glm::quatLookAtLH(glm::normalize(characterPos - cameraPos), glm::vec3(0, 1, 0));
    targetGO->SetRotation(lookAtQuat);
}

void PlayerController::SetCameraSphericalPos(float xDelta, float yDelta)
{
    float rotSpeed = Time::DeltaTime() * rotateSpeed;
    phi += xDelta * rotSpeed;
    theta += yDelta * rotSpeed;
    glm::vec3 sphPos = {0, 0, 0};
    float cosTheta = glm::cos(theta);
    sphPos.x = glm::cos(phi) * cosTheta;
    sphPos.z = glm::sin(phi) * cosTheta;
    sphPos.y = sin(theta);
    glm::vec3 finalSphOffset = sphPos * cameraDistance;
    target->GetGameObject()->SetPosition(GetGameObject()->GetPosition() + finalSphOffset);
}

void PlayerController::AutoRotate() {}
