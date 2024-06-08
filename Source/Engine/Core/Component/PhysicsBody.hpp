#pragma once

#include "Component.hpp"
#include "Core/Scene/PhysicsLayer.hpp"
#include <memory>

// clang-format off
#include <Jolt/Jolt.h>
// clang-format on
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionProperties.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

class RenderingScene;

class PhysicsBody : public Component
{
    DECLARE_OBJECT();

public:
    PhysicsBody();
    PhysicsBody(GameObject* owner);
    ~PhysicsBody() override;

    void SetAsSphere(float radius);
    void SetAsBox(glm::vec3 extent);
    void SetLayer(PhysicsLayer layer);
    void SetMotionType(JPH::EMotionType motionType);

    JPH::EMotionType GetMotionType() const
    {
        return motionType;
    }

    glm::vec4 GetBodyScale() const
    {
        return bodyScale;
    }

    PhysicsLayer GetLayer() const
    {
        return layer;
    }

    float GetGravityFactory() const
    {
        return gravityFactor;
    }

    void Awake() override;

    void SetGravityFactor(float f);

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

    void UpdateGameObject();

    // set this to true, the physics scene will try to draw this physics body in this frame
    bool debugDrawRequest = false;

    JPH::Body* GetBody()
    {
        return body;
    }

private:
    glm::vec4 bodyScale = {0.5, 0.5, 0.5, 0.5};
    PhysicsLayer layer = PhysicsLayer::Scene;
    float gravityFactor = 0.0f;
    JPH::EMotionType motionType = JPH::EMotionType::Static;

    JPH::ShapeRefC shapeRef;
    JPH::Body* body = nullptr;

    void EnableImple() override;
    void DisableImple() override;
    bool SetShape(JPH::ShapeSettings& shape);

    JPH::BodyInterface* GetBodyInterface();

    void Init();
};
