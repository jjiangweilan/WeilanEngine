#include "PlayerController.hpp"
#include "Core/Component/AnimationPlayer.hpp"
#include "Core/Component/Camera.hpp"
#include "Core/Component/PhysicsBody.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/PhysicsScene.hpp"
#include "Core/Scene/Scene.hpp"
#include "Core/Time.hpp"
#include "Gameplay/Input.hpp"
#include <spdlog/spdlog.h>
#if ENGINE_EDITOR
#include "Editor/HudDebug.hpp"
#endif

// clang-format off
#include <Jolt/Jolt.h>
// clang-format on
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
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
    s->Serialize("characterCapsuleShapeHalfHeight", characterCapsuleShapeHalfHeight);
    s->Serialize("characterCapsuleShapeRadius", characterCapsuleShapeRadius);
    s->Serialize("rootMotionAnimationPlayer", rootMotionAnimationPlayer);
    s->Serialize("rotationRoot", rotationRoot);
}
void PlayerController::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("target", target);
    s->Deserialize("movementSpeed", movementSpeed);
    s->Deserialize("cameraDistance", cameraDistance);
    s->Deserialize("rotateSpeed", rotateSpeed);
    s->Deserialize("jumpImpulse", jumpImpulse);
    s->Deserialize("characterCapsuleShapeHalfHeight", characterCapsuleShapeHalfHeight);
    s->Deserialize("characterCapsuleShapeRadius", characterCapsuleShapeRadius);
    s->Deserialize("rootMotionAnimationPlayer", rootMotionAnimationPlayer);
    s->Deserialize("rotationRoot", rotationRoot);
}

void PlayerController::PrePhysicsTick()
{
    if (valid)
    {
        UpdateCharacter();
    }
}

void PlayerController::HandleInput()
{
    // handle input
    auto cameraGO = target->GetGameObject();
    if (cameraGO == GetGameObject())
        return;

    character->UpdateGroundVelocity();
    // check current vertical moving directional
    float currentVerticalVelocity = character->GetLinearVelocity().Dot(character->GetUp());
    JPH::Vec3 groundVelocity = character->GetGroundVelocity();
    bool movingTowardsGround = (currentVerticalVelocity - groundVelocity.GetY()) < 0.1f;

    // don't lose gravity and vertical velocity
    velocity = glm::vec3{0, 0, 0};
    velocity.y = currentVerticalVelocity;
    auto gravity = GetScene()->GetPhysicsScene().GetPhysicsSystem().GetGravity() * Time::DeltaTime() * gravityScale;
    velocity += glm::vec3(gravity.GetX(), gravity.GetY(), gravity.GetZ());

    if (character->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround)
    {
        velocity = {groundVelocity.GetX(), groundVelocity.GetY(), groundVelocity.GetZ()};

        if (Input::GetSingleton().Jump() && movingTowardsGround)
        {
            velocity.y += 10.0f;
        }
    }

    // set movement
    float mx, my;
    Input::GetSingleton().GetMovement(mx, my);

    if (mx != 0 || my != 0)
    {
        auto forward = -cameraGO->GetForward();
        auto right = cameraGO->GetRight();

        // mute y
        forward.y = 0;
        right.y = 0;
        glm::vec3 dir = glm::normalize(my * forward + mx * right);
        // if (rootMotionAnimationPlayer)
        // {
        //     velocity += dir * glm::length(rootMotionAnimationPlayer->GetRootMotionDelta() / Time::DeltaTime()) ;
        // }
        // else
        // {
        velocity += dir * movementSpeed * Time::DeltaTime();
        // }
    }

    character->SetLinearVelocity({velocity.x, velocity.y, velocity.z});
}

void PlayerController::OnStart()
{
    valid = false;
    if (target == nullptr)
    {
        return;
    }
    auto targetGO = target->GetGameObject();
    if (targetGO == GetGameObject())
    {
        return;
    }

    // set camera's initial position
    SetCameraSphericalPos(0, 0);

    // set camera's initial rotation
    auto characterPos = GetGameObject()->GetPosition();
    glm::vec3 cameraPos = targetGO->GetPosition();
    auto lookAtQuat = glm::quatLookAtLH(glm::normalize(characterPos - cameraPos), glm::vec3(0, 1, 0));
    targetGO->SetRotation(lookAtQuat);

    CreateCharacterPhysicsShape();

    valid = true;
}

void PlayerController::OnDestroy()
{
    DestroyCharacterPhysicsShape();
}

glm::vec3 PlayerController::CalculateSphericalPosition(float xDelta, float yDelta, float& phi, float& theta)
{
    phi += xDelta;
    theta += yDelta;
    glm::vec3 sphPos = {0, 0, 0};
    float cosTheta = glm::cos(theta);
    sphPos.x = glm::cos(phi) * cosTheta;
    sphPos.z = glm::sin(phi) * cosTheta;
    sphPos.y = -sin(theta);

    return sphPos;
}

void PlayerController::SetCameraSphericalPos(float xDelta, float yDelta)
{
    glm::vec3 sphPos = CalculateSphericalPosition(0, 0, cameraPhi, cameraTheta);
    glm::vec3 finalSphOffset = sphPos * cameraDistance;
    auto playerPos = GetGameObject()->GetPosition();

    playerPos.y = 0;
    target->GetGameObject()->SetPosition(playerPos + finalSphOffset);
}

void PlayerController::OnEnable() {}

void PlayerController::OnDisable() {}

void PlayerController::UpdateCharacter()
{
    auto& pscene = GetScene()->GetPhysicsScene();
    auto& physicsSystem = pscene.GetPhysicsSystem();
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
        pscene.DeltaTime,
        -character->GetUp() * physicsSystem.GetGravity().Length() * gravityScale,
        update_settings,
        physicsSystem.GetDefaultBroadPhaseLayerFilter(static_cast<JPH::ObjectLayer>(PhysicsLayer::Moving)),
        physicsSystem.GetDefaultLayerFilter(static_cast<JPH::ObjectLayer>(PhysicsLayer::Moving)),
        {},
        {},
        tempAllocator
    );
}

void PlayerController::SetRootMotionAnimationPlayer(AnimationPlayer* animationPlayer)
{
    this->rootMotionAnimationPlayer = animationPlayer;
}

void PlayerController::Tick()
{
    // update character
    if (valid && character)
    {
        HandleInput();

        // update player and camera position
        auto pos = character->GetPosition();
        GetGameObject()->SetPosition(
            {pos.GetX(), pos.GetY() - characterCapsuleShapeHalfHeight - characterCapsuleShapeRadius, pos.GetZ()}
        );

        float lx, ly;
        Input::GetSingleton().GetLookAround(lx, ly);

        /* camera update */
        // set camera lookat (camera position)
        auto characterPos = GetGameObject()->GetPosition();
        characterPos.y = playerHorizonPos;
        // set camera lookat character
        SetCameraSphericalPos(-lx, -ly);
        auto cameraGO = target->GetGameObject();
        glm::vec3 cameraPos = cameraGO->GetPosition();
        auto lookAtQuat = glm::quatLookAt(glm::normalize(characterPos - cameraPos), glm::vec3(0, 1, 0));
        cameraGO->SetLocalRotation(lookAtQuat);

        UpdatePlayerLookAt(-lx);


        /****** update animation ******/
        if (rootMotionAnimationPlayer)
        {
            float speed = glm::length(velocity);
            float blendFactor = glm::mix(-blendFactorScale, blendFactorScale, speed);
            animationBlendFactor += blendFactor;
            animationBlendFactor = glm::clamp(animationBlendFactor, 0.f, 1.f);
            rootMotionAnimationPlayer->SetBlendClipFactor(animationBlendFactor);
        }
    }
}

void PlayerController::UpdatePlayerLookAt(float xDelta)
{
    if (rotationRoot)
    {
        glm::vec3 rot =
            CalculateSphericalPosition(xDelta * Time::DeltaTime() * playerRotationSpeed, 0, playerPhi, playerTheta);
        auto v = velocity;
        v.y = 0;
        float speed = glm::length(v);
        bool hasVelocity = speed != 0;
        if (hasVelocity)
        {
            auto lookAtRot = glm::quatLookAt(-v / speed, {0, 1, 0});
            rotationRoot->SetRotation(lookAtRot);
        }
    }
}

void PlayerController::CreateCharacterPhysicsShape()
{
    auto scene = GetScene();
    if (scene == nullptr)
    {
        spdlog::error("failed to CreateCharacterPhysicsShape, because scene is null");
        return;
    }

    // create character
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

    // create shape
    SetCharacterCapsuleShapeInternal();

    auto pos = gameObject->GetPosition();
    character->SetPosition({pos.x, pos.y, pos.z});
}

void PlayerController::SetCharacterCapsuleShape(float height, float radius)
{
    characterCapsuleShapeHalfHeight = height;
    characterCapsuleShapeRadius = radius;
}

void PlayerController::SetCharacterCapsuleShapeInternal()
{
    JPH::CapsuleShapeSettings ss(characterCapsuleShapeHalfHeight, characterCapsuleShapeRadius);
    standingShape = ss.Create().Get();

    auto& bSystem = GetScene()->GetPhysicsScene().GetPhysicsSystem();
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

void PlayerController::OnDrawGizmos() {}

void PlayerController::DestroyCharacterPhysicsShape()
{
    if (standingShape)
        standingShape->Release();

    character = nullptr;
}
