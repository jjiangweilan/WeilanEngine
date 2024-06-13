#include "PlayerController.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/PhysicsBody.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/PhysicsScene.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Time.hpp"
#include "Gameplay/Input.hpp"
#include "Jolt/Physics/Collision/CollisionCollectorImpl.h"
#include <spdlog/spdlog.h>

// clang-format off
#include <Jolt/Jolt.h>
// clang-format on
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#include <Jolt/Physics/Collision/ShapeCast.h>

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
    s->Serialize("cameraDistance", cameraDistance);
    s->Serialize("rotateSpeed", rotateSpeed);
    s->Serialize("jumpImpulse", jumpImpulse);
}
void PlayerController::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("target", target);
    s->Deserialize("movementSpeed", movementSpeed);
    s->Deserialize("cameraDistance", cameraDistance);
    s->Deserialize("rotateSpeed", rotateSpeed);
    s->Deserialize("jumpImpulse", jumpImpulse);
}
PhysicsBody* otherBodyID{};
void PlayerController::Tick()
{
    if (valid)
    {
        HandleInput();
        UpdateCharacter();
    }
}

void PlayerController::HandleInput()
{
    // handle input

    auto cameraGO = target->GetGameObject();
    if (cameraGO == GetGameObject())
        return;

    // set movement
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
    glm::vec3 velocity = forward + right;

    // don't lose gravity
    velocity.y = character->GetLinearVelocity().GetY();
    // jump
    if (Input::GetSingleton().Jump() && character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround)
    {
        velocity.y = 10;
    }
    character->SetLinearVelocity({velocity.x, velocity.y, velocity.z});

    /* camera update */
    // set camera lookat (camera position)
    auto characterPos = GetGameObject()->GetPosition();
    float lx, ly;
    Input::GetSingleton().GetLookAround(lx, ly);
    SetCameraSphericalPos(-lx, -ly);

    // set camera lookat character
    glm::vec3 cameraPos = cameraGO->GetPosition();
    auto lookAtQuat = glm::quatLookAtLH(glm::normalize(characterPos - cameraPos), glm::vec3(0, 1, 0));
    cameraGO->SetRotation(lookAtQuat);
}

void PlayerController::Awake()
{
    if (target == nullptr)
    {
        valid = false;
        return;
    }

    auto targetGO = target->GetGameObject();
    if (targetGO == GetGameObject())
    {
        valid = false;
        return;
    }

    // set camera's initial position
    SetCameraSphericalPos(0, 0);

    // set camera's initial rotation
    auto characterPos = GetGameObject()->GetPosition();
    glm::vec3 cameraPos = targetGO->GetPosition();
    auto lookAtQuat = glm::quatLookAtLH(glm::normalize(characterPos - cameraPos), glm::vec3(0, 1, 0));
    targetGO->SetRotation(lookAtQuat);

    valid = true;
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

void PlayerController::EnableImple()
{
    if (auto scene = GetScene())
    {
        JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings();
        settings->mMaxSlopeAngle = maxSlopeAngle;
        settings->mMaxStrength = maxStrength;
        settings->mShape = standingShape;
        settings->mBackFaceMode = JPH::EBackFaceMode::IgnoreBackFaces;
        settings->mCharacterPadding = characterPadding;
        settings->mPenetrationRecoverySpeed = penetrationRecoverySpeed;
        settings->mPredictiveContactDistance = predictiveContactDistance;
        settings->mSupportingVolume = JPH::Plane(
            JPH::Vec3::sAxisY(),
            -characterRadiusStanding
        ); // Accept contacts that touch the lower sphere of the capsule
        character = new JPH::CharacterVirtual(
            settings,
            JPH::RVec3::sZero(),
            JPH::Quat::sIdentity(),
            &scene->GetPhysicsScene().GetPhysicsSystem()
        );
        character->SetListener(this);

        auto& bSystem = scene->GetPhysicsScene().GetPhysicsSystem();

        auto extent = GetGameObject()->GetScale();
        JPH::BoxShapeSettings s({extent.x, extent.y, extent.z});
        standingShape = s.Create().Get();
        character->SetShape(
            standingShape,
            1.5f * bSystem.GetPhysicsSettings().mPenetrationSlop,
            bSystem.GetDefaultBroadPhaseLayerFilter(static_cast<JPH::ObjectLayer>(PhysicsLayer::Moving)),
            bSystem.GetDefaultLayerFilter(static_cast<JPH::ObjectLayer>(PhysicsLayer::Moving)),
            {},
            {},
            tempAllocator
        );
    }
}

void PlayerController::DisableImple()
{
    if (standingShape)
        standingShape->Release();
}

void PlayerController::UpdateCharacter()
{
    auto& physicsSystem = GetScene()->GetPhysicsScene().GetPhysicsSystem();
    // Settings for our update function
    JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
    if (!enableStickToFloor)
        update_settings.mStickToFloorStepDown = JPH::Vec3::sZero();
    else
        update_settings.mStickToFloorStepDown = -character->GetUp() * update_settings.mStickToFloorStepDown.Length();
    if (!enableWalkStairs)
        update_settings.mWalkStairsStepUp = JPH::Vec3::sZero();
    else
        update_settings.mWalkStairsStepUp = character->GetUp() * update_settings.mWalkStairsStepUp.Length();

    // Update the character position
    character->ExtendedUpdate(
        Time::DeltaTime(),
        -character->GetUp() * physicsSystem.GetGravity().Length(),
        update_settings,
        physicsSystem.GetDefaultBroadPhaseLayerFilter(static_cast<JPH::ObjectLayer>(PhysicsLayer::Moving)),
        physicsSystem.GetDefaultLayerFilter(static_cast<JPH::ObjectLayer>(PhysicsLayer::Moving)),
        {},
        {},
        tempAllocator
    );
}
