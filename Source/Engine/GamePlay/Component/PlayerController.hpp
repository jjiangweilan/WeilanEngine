#pragma once
#include "Core/Component/Component.hpp"
// clang-format off
#include <Jolt/Jolt.h>
// clang-format on
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/ContactListener.h>

class AnimationPlayer;
class Camera;
class PhysicsBody;
class PlayerController : public Component, JPH::CharacterContactListener
{
    DECLARE_OBJECT();

public:
    float movementSpeed = 1400.f;
    float rotateSpeed = 0.3f;
    float jumpImpulse = 3.0f;
    float gravityScale = 10.0f;

    float blendFactorScale = 1.3f;

    // physicalCharacter creation setting
    float maxSlopeAngle = JPH::DegreesToRadians(45.0f);
    float maxStrength = 100.f;
    float characterPadding = 0.02f;
    float penetrationRecoverySpeed = 1.0f;
    float predictiveContactDistance = 0.1f;
    float characterRadiusStanding = 0.3f;
    bool enableWalkStairs = true;
    bool enableStickToFloor = true;
    float playerHorizonPos = 0.0f;

    // Usages:
    // 1. rotate by PlayerController to make the player facing to moving direction
    ObjPtr<GameObject> rotationRoot = nullptr;

    /******** Camera *********/
    float cameraTheta = -0.8;
    float cameraPhi = 0;
    float cameraElasticity = 6.0f;
    float cameraMinElasticity = 3.65f;
    float cameraOffset = 8.0f;

    JPH::RefConst<JPH::Shape> standingShape;

    PlayerController();
    PlayerController(GameObject* gameObject);
    ~PlayerController() override {};

    std::unique_ptr<Component> Clone(GameObject& owner) override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    const std::string& GetName() override;
    void PrePhysicsTick() override;
    void Tick() override;

    /***** In-Game Control ******/
    void SetRootMotionAnimationPlayer(AnimationPlayer* animationPlayer);
    AnimationPlayer* GetRootMotionAnimationPlayer() const { return rootMotionAnimationPlayer; }
    void SetCharacterCapsuleShape(float halfHeight, float radius);
    float GetCharacterCapsuleShapeHalfHeight() const { return characterCapsuleShapeHalfHeight; }
    float GetCharacterCapsuleShapeRadius() const { return characterCapsuleShapeRadius; }

    void SetCamera(Camera* camera) { this->camera = camera; }
    Camera* GetCamera() { return camera; }

private:
    float characterCapsuleShapeHalfHeight = 1.75;
    float characterCapsuleShapeRadius = 0.8;
    ObjPtr<Camera> camera = nullptr;
    ObjPtr<AnimationPlayer> rootMotionAnimationPlayer = nullptr;

    /**** Runtime Data ****/
    float3 cameraFollowPosition;
    float3 velocity{};
    float playerTheta = 0;
    float playerPhi = 0;
    // camera rotation around player
    bool valid = false;
    JPH::Ref<JPH::CharacterVirtual> physicalCharacter;
    JPH::TempAllocatorImpl tempAllocator = JPH::TempAllocatorImpl(10 * 10 * 1024);
    float animationBlendFactor = 0.0f;

    void SetCameraSphericalPos(float xDelta, float yDelta);
    void UpdateCameraTransform(float xDelta, float yDelta, float3 preFramePlayerPos);
    void UpdateCharacterLookAt(float xDelta);
    void ContactAddedEventCallback(
        PhysicsBody* self, PhysicsBody* other, const JPH::ContactManifold&, JPH::ContactSettings&
    );
    void ContactRemovedEventCallback(PhysicsBody* self, PhysicsBody* other);

    void OnStart() override;
    void OnDestroy() override;
    void OnEnable() override;
    void OnDisable() override;
    void OnDrawGizmos() override;
    void UpdatePhysicalCharacterVelocity();

    void CreateCharacterPhysicsShape();
    void SetCharacterCapsuleShapeInternal();
    void DestroyCharacterPhysicsShape();
    void UpdateCharacter();
    glm::vec3 CalculateSphericalPosition(float xDelta, float yDelta, float& phi, float& theta);
};
