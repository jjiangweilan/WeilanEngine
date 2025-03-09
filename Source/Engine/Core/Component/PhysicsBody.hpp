#pragma once

#include "Component.hpp"
#include "Core/Scene/PhysicsLayer.hpp"
#include <memory>

// clang-format off
#include <Jolt/Jolt.h>
// clang-format on
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionProperties.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

class PhysicsScene;

enum class PhysicsBodyShapes
{
    Box,
    Sphere,
    Mesh,
    Capsule,
    Compound
};

class PhysicsBody : public Component
{
    DECLARE_OBJECT();

public:
    PhysicsBody();
    PhysicsBody(GameObject* owner);
    ~PhysicsBody() override;

    PhysicsScene* GetPhysicsScene();
    void SetShape(PhysicsBodyShapes shape);
    PhysicsBodyShapes GetShape() { return shapeType; }
    void SetLayer(PhysicsLayer layer);
    void SetMotionType(JPH::EMotionType motionType);

    void SetCapsuleShape(float halfHeight, float radius)
    {
        bodyScale.x = halfHeight;
        bodyScale.y = radius;
    }

    void GetCapsuleShape(float& halfHeight, float& radius)
    {
        halfHeight = bodyScale.x;
        radius = bodyScale.y;
    }

    JPH::EMotionType GetMotionType() const { return motionType; }

    glm::vec4 GetBodyScale() const { return bodyScale; }

    void SetBodyOffset(const glm::vec4& offset)
    {
        this->bodyOffset = offset;
        if (recreateShape)
            recreateShape();
    }

    const glm::vec4& GetBodyOffset() const { return bodyOffset; }
    void SetBodyScale(const glm::vec4& scale)
    {
        this->bodyScale = scale;
        if (recreateShape)
            recreateShape();
    }

    bool IsSensor() { return isSensor; }
    void SetSensor(bool isSensor);

    PhysicsLayer GetLayer() const { return layer; }

    float GetGravityFactory() const { return gravityFactor; }

    void RegisterContactAddedEvent(
        const std::function<void(PhysicsBody*, PhysicsBody*, const JPH::ContactManifold&, JPH::ContactSettings&)>& f
    )
    {
        contactAddedCallbacks.push_back(f);
    }

    void RegisterContactRemovedEvent(const std::function<void(PhysicsBody*, PhysicsBody*)>& f)
    {
        contactRemovedCallbacks.push_back(f);
    }

    void InvokeContactRemovedEvent(PhysicsBody* other)
    {
        for (auto& f : contactRemovedCallbacks)
        {
            f(this, other);
        }
    }

    void InvokeContactAddedEvent(
        PhysicsBody* other, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings
    )
    {
        for (auto& f : contactAddedCallbacks)
        {
            f(this, other, manifold, settings);
        }
    }

    void SetLinearVelocity(const glm::vec3& velocity);
    glm::vec3 GetLinearVelocity();
    void AddForce(const glm::vec3& force);
    void AddImpulse(const glm::vec3& impulse);
    void SetGravityFactor(float f);
    void UpdateGameObject();
    JPH::Body* GetBody() { return body; }
    JPH::Ref<JPH::Shape> GetShapeRef() { return shapeRef; }

    void OnStart() override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

    // set this to true, the physics scene will try to draw this physics body in this frame
    bool debugDrawRequest = false;

private:
    using ContactAddedEventCallbackType =
        std::function<void(PhysicsBody*, PhysicsBody*, const JPH::ContactManifold&, JPH::ContactSettings&)>;
    using ContactRemovedEventCallbackType = std::function<void(PhysicsBody*, PhysicsBody*)>;
    glm::vec4 bodyScale = {1.0, 1.0, 1.0, 1.0};
    glm::vec4 bodyOffset = {0.0, 0.0, 0.0, 0.0};
    PhysicsLayer layer = PhysicsLayer::Scene;
    float gravityFactor = 0.0f;
    bool isSensor = false;

    JPH::EMotionType motionType = JPH::EMotionType::Static;
    JPH::Ref<JPH::Shape> shapeRef;
    JPH::Body* body = nullptr;
    PhysicsBodyShapes shapeType = PhysicsBodyShapes::Mesh;

    std::function<bool()> recreateShape = nullptr;
    std::vector<ContactAddedEventCallbackType> contactAddedCallbacks = {};
    std::vector<ContactRemovedEventCallbackType> contactRemovedCallbacks = {};

    void OnEnable() override;
    void OnDisable() override;
    bool SetShape(JPH::ShapeSettings& shape);
    void TransformChanged() override;
    void UpdateBodyPositionAndRotation();
    JPH::BodyInterface* GetBodyInterface();
    void Init();
    bool GenerateTrianglesFromMeshRenderer(JPH::Array<JPH::Triangle>& triangles);
    bool SetAsSphere();
    bool SetAsCapsule();
    bool SetAsMeshRenderer();
    bool SetAsBox();
    bool SetAsCompound();
};
