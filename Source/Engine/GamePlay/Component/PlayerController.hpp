#pragma once
#include "Core/Component/Component.hpp"
// clang-format off
#include <Jolt/Jolt.h>
// clang-format on
#include <Jolt/Physics/Collision/ContactListener.h>

class Camera;
class PhysicsBody;
class PlayerController : public Component
{
    DECLARE_OBJECT();

public:
    float movementSpeed = 1.0f;
    float rotateSpeed = 0.3f;
    float cameraDistance = 5.0f;
    float jumpImpulse = 3.0f;

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

    PhysicsBody* pbody;
    bool valid = false;
    int isOnGround = 0;

    void SetCameraSphericalPos(float xDelta, float yDelta);
    void
    ContactAddedEventCallback(PhysicsBody* self, PhysicsBody* other, const JPH::ContactManifold&, JPH::ContactSettings&);
    void ContactRemovedEventCallback(PhysicsBody* self, PhysicsBody* other);
    bool IsOnGround();
};
