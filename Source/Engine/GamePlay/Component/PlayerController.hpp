#pragma once
#include "Core/Component/Component.hpp"
// clang-format off
#include <Jolt/Jolt.h>
// clang-format on
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/ContactListener.h>

class Camera;
class PhysicsBody;
class PlayerController : public Component, JPH::CharacterContactListener
{
    DECLARE_OBJECT();

public:
    float movementSpeed = 1.0f;
    float rotateSpeed = 0.3f;
    float cameraDistance = 5.0f;
    float jumpImpulse = 3.0f;

    // character creation setting
    float maxSlopeAngle = JPH::DegreesToRadians(45.0f);
    float maxStrength = 100.f;
    float characterPadding = 0.02f;
    float penetrationRecoverySpeed = 1.0f;
    float predictiveContactDistance = 0.1f;
    float characterRadiusStanding = 0.3f;
    bool enableWalkStairs = true;
    bool enableStickToFloor = true;

    JPH::RefConst<JPH::Shape> standingShape;

    PlayerController();
    PlayerController(GameObject* gameObject);
    ~PlayerController() override{};

    std::unique_ptr<Component> Clone(GameObject& owner) override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    const std::string& GetName() override;
    void Tick() override;

    void SetCamera(Camera* camera)
    {
        this->target = camera;
    }

    Camera* GetCamera()
    {
        return target;
    }

    void Awake() override;

private:
    Camera* target = nullptr;
    // camera rotation around player
    float theta, phi;

    bool valid = false;
    JPH::Ref<JPH::CharacterVirtual> character;
    JPH::TempAllocatorImpl tempAllocator = JPH::TempAllocatorImpl(10 * 10 * 1024);

    void SetCameraSphericalPos(float xDelta, float yDelta);
    void
    ContactAddedEventCallback(PhysicsBody* self, PhysicsBody* other, const JPH::ContactManifold&, JPH::ContactSettings&);
    void ContactRemovedEventCallback(PhysicsBody* self, PhysicsBody* other);

    void EnableImple() override;
    void DisableImple() override;
    void HandleInput();

    void UpdateCharacter();
};
