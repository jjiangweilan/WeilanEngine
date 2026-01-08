#include "PlayerController.hpp"
#include "Engine/Core/Time.hpp"
#include "Engine/Game/Input.hpp"
#include "Engine/Runtime/Object/Component/AnimationPlayer.hpp"
#include "Engine/Runtime/Object/Component/Camera.hpp"
#include "Engine/Runtime/Object/Component/PhysicsBody.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/SceneManager/PhysicsScene.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
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

DEFINE_OBJECT(Component, PlayerController, "A14D66B4-47AB-4703-BEAA-06EBD285034F");

PlayerController::PlayerController() : Component(nullptr) {}

PlayerController::PlayerController(GameObject* gameObject) : Component(gameObject) {}

const std::string& PlayerController::GetName() const
{
    static std::string name = "PlayerController";
    return name;
}

std::unique_ptr<Component> PlayerController::Clone(GameObject& owner)
{
    std::unique_ptr<PlayerController> clone = std::make_unique<PlayerController>(&owner);
    clone->SetCamera(camera);
    clone->movementSpeed = movementSpeed;

    return clone;
}

void PlayerController::Serialize(Serializer* s) const
{
    Component::Serialize(s);
    s->Serialize("camera", camera);
    s->Serialize("movementSpeed", movementSpeed);
    s->Serialize("cameraDistance", cameraOffset);
    s->Serialize("rotateSpeed", rotateSpeed);
    s->Serialize("jumpImpulse", jumpImpulse);
    s->Serialize("characterCapsuleShapeHalfHeight", characterCapsuleShapeHalfHeight);
    s->Serialize("characterCapsuleShapeRadius", characterCapsuleShapeRadius);
    s->Serialize("rootMotionAnimationPlayer", rootMotionAnimationPlayer);
    s->Serialize("rotationRoot", rotationRoot);
    s->Serialize("cameraElasticity", cameraElasticity);
    SERIALIZE(s, cameraMinElasticity);
}
void PlayerController::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
    s->Deserialize("camera", camera);
    s->Deserialize("movementSpeed", movementSpeed);
    s->Deserialize("cameraDistance", cameraOffset);
    s->Deserialize("rotateSpeed", rotateSpeed);
    s->Deserialize("jumpImpulse", jumpImpulse);
    s->Deserialize("characterCapsuleShapeHalfHeight", characterCapsuleShapeHalfHeight);
    s->Deserialize("characterCapsuleShapeRadius", characterCapsuleShapeRadius);
    s->Deserialize("rootMotionAnimationPlayer", rootMotionAnimationPlayer);
    s->Deserialize("rotationRoot", rotationRoot);
    s->Deserialize("cameraElasticity", cameraElasticity);
    DESERIALIZE(s, cameraMinElasticity);
}

void PlayerController::PrePhysicsTick()
{
    if (valid)
    {
        UpdatePhysicalCharacterVelocity();
        UpdateCharacter();

        // Get player's position before updating it
        auto preFramePlayerPosition = GetGameObject()->GetPosition();

        // Prepare data
        float lx, ly;
        Input::GetLookAround(lx, ly);

        // Update camera transform
        UpdateCameraTransform(lx, ly, preFramePlayerPosition);

        // Update rotation of physicalCharacter's visual representation
        UpdateCharacterLookAt(-lx);
    }
}

void PlayerController::UpdatePhysicalCharacterVelocity()
{
    // Get CameraGO
    auto cameraGO = camera->GetGameObject();
    if (cameraGO == GetGameObject())
        return;

    physicalCharacter->UpdateGroundVelocity();
    // check current vertical moving directional
    float currentVerticalVelocity = physicalCharacter->GetLinearVelocity().Dot(physicalCharacter->GetUp());
    JPH::Vec3 groundVelocity = physicalCharacter->GetGroundVelocity();
    bool movingTowardsGround = (currentVerticalVelocity - groundVelocity.GetY()) < 0.1f;

    // don't lose gravity and vertical velocity
    velocity = glm::vec3{0, 0, 0};
    velocity.y = currentVerticalVelocity;
    auto vDelta = GetScene()->GetPhysicsScene().GetPhysicsSystem().GetGravity() *
                  GetScene()->GetPhysicsScene().GetDeltaTime() * gravityScale;
    velocity += glm::vec3(vDelta.GetX(), vDelta.GetY(), vDelta.GetZ());

    if (physicalCharacter->GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround)
    {
        velocity = {groundVelocity.GetX(), groundVelocity.GetY(), groundVelocity.GetZ()};

        if (Input::Jump() && movingTowardsGround)
        {
            velocity.y += 10.0f;
        }
    }

    // set movement
    float mx, my;
    Input::GetMovement(mx, my);

    if (mx != 0 || my != 0)
    {
        auto forward = -cameraGO->GetForward();
        auto right = cameraGO->GetRight();

        // mute y
        forward.y = 0;
        right.y = 0;
        glm::vec3 dir = glm::normalize(my * forward + mx * right);
        velocity += dir * movementSpeed;
    }

    physicalCharacter->SetLinearVelocity({velocity.x, velocity.y, velocity.z});
}

void PlayerController::OnAwake()
{
    valid = false;
    if (camera == nullptr)
    {
        return;
    }

    auto targetGO = camera ? camera->GetGameObject() : nullptr;
    if (targetGO == nullptr || targetGO == GetGameObject())
    {
        return;
    }

    // set camera's initial rotation
    auto characterPos = GetGameObject()->GetPosition();
    glm::vec3 cameraPos = targetGO->GetPosition();
    auto lookAtQuat = glm::quatLookAtLH(glm::normalize(characterPos - cameraPos), glm::vec3(0, 1, 0));
    targetGO->SetRotation(lookAtQuat);

    // set camera's initial position
    SetCameraSphericalPos(0, 0);
    cameraFollowPosition = characterPos;

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
    auto cameraGO = camera->GetGameObject();

    xDelta *= rotateSpeed;
    yDelta *= rotateSpeed;
    auto playerPos = GetGameObject()->GetPosition();
    float3 sphPos = CalculateSphericalPosition(xDelta, yDelta, cameraPhi, cameraTheta);
    float3 finalSphOffset = sphPos * cameraOffset;

    cameraGO->SetPosition(playerPos + finalSphOffset);
}

void PlayerController::UpdateCameraTransform(float xDelta, float yDelta, float3 preFramePlayerPos)
{
    auto cameraGO = camera->GetGameObject();

    xDelta *= rotateSpeed;
    yDelta *= rotateSpeed;

    // Data
    auto playerPos = GetGameObject()->GetPosition();
    float3 cameraOldPos = cameraGO->GetPosition();
    float physicsDeltatime = GetScene()->GetPhysicsScene().GetDeltaTime();

    // Calculate new spherical position relative to the player
    float3 newSphPos = CalculateSphericalPosition(xDelta, yDelta, cameraPhi, cameraTheta);
    float3 sphOffset = newSphPos * cameraOffset;

    // Update camera position
    float elasticity = glm::abs(glm::length(cameraOldPos - playerPos) - cameraOffset) * cameraElasticity;
    elasticity = glm::max(elasticity, cameraMinElasticity);
    cameraFollowPosition = glm::lerp(cameraFollowPosition, playerPos, glm::saturate(elasticity * physicsDeltatime));
    float3 cameraNewPosition = cameraFollowPosition + sphOffset;
    cameraGO->SetPosition(cameraNewPosition);

    // Update camera rotation
    auto lookAtDir = glm::normalize(cameraFollowPosition - cameraNewPosition);
    auto rot = glm::quatLookAt(lookAtDir, float3(0, 1, 0));
    cameraGO->SetLocalRotation(rot);
}

void PlayerController::UpdateCharacter()
{
    auto& pscene = GetScene()->GetPhysicsScene();
    auto& physicsSystem = pscene.GetPhysicsSystem();
    // Settings for our update function
    JPH::CharacterVirtual::ExtendedUpdateSettings update_settings;
    if (!enableStickToFloor)
        update_settings.mStickToFloorStepDown = JPH::Vec3::sZero();
    else
        update_settings.mStickToFloorStepDown =
            -physicalCharacter->GetUp() * update_settings.mStickToFloorStepDown.Length();
    if (!enableWalkStairs)
        update_settings.mWalkStairsStepUp = JPH::Vec3::sZero();
    else
        update_settings.mWalkStairsStepUp = physicalCharacter->GetUp() * update_settings.mWalkStairsStepUp.Length();

    // Update the physicalCharacter position
    physicalCharacter->ExtendedUpdate(
        pscene.GetDeltaTime(),
        physicsSystem.GetGravity() * gravityScale,
        update_settings,
        physicsSystem.GetDefaultBroadPhaseLayerFilter(static_cast<JPH::ObjectLayer>(PhysicsObjectLayers::Dynamic)),
        physicsSystem.GetDefaultLayerFilter(static_cast<JPH::ObjectLayer>(PhysicsObjectLayers::Dynamic)),
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
    // update physicalCharacter
    if (valid && physicalCharacter)
    {
        // Update physicalCharacter's position
        auto pos = physicalCharacter->GetPosition();
        auto characterPos =
            float3{pos.GetX(), pos.GetY() - characterCapsuleShapeHalfHeight - characterCapsuleShapeRadius, pos.GetZ()};
        GetGameObject()->SetPosition(characterPos);

        // Update animation blend factor
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

void PlayerController::UpdateCharacterLookAt(float xDelta)
{
    if (rotationRoot)
    {
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

    // create physicalCharacter
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
    physicalCharacter = new JPH::CharacterVirtual(
        settings,
        JPH::RVec3::sZero(),
        JPH::Quat::sIdentity(),
        &scene->GetPhysicsScene().GetPhysicsSystem()
    );
    physicalCharacter->SetListener(this);

    // create shape
    SetCharacterCapsuleShapeInternal();

    auto pos = gameObject->GetPosition();
    physicalCharacter->SetPosition({pos.x, pos.y, pos.z});
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
    physicalCharacter->SetShape(
        standingShape,
        1.5f * bSystem.GetPhysicsSettings().mPenetrationSlop,
        bSystem.GetDefaultBroadPhaseLayerFilter(static_cast<JPH::ObjectLayer>(PhysicsObjectLayers::Dynamic)),
        bSystem.GetDefaultLayerFilter(static_cast<JPH::ObjectLayer>(PhysicsObjectLayers::Dynamic)),
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

    physicalCharacter = nullptr;
}
