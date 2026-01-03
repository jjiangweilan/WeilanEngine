#pragma once

#include "Component.hpp"
#include "Engine/Runtime/System/SceneManager/PhysicsLayer.hpp"
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

enum class PhysicsContactEvent
{
    Added,
    Removed,
    Persisted
};

class GameScript;
struct PhysicsLuaCallback : public Serializable
{
    ObjPtr<GameScript> gameScript;
    std::string callback;

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
};

class [[LuaClass]] PhysicsBody : public Component
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

    bool ShouldKinematicGenerateContactPointsWithNonDynamic() const
    {
        return kinematicGenerateContactPointsWithNonDynamic;
    }
    void SetKinematicCollideWithNonDynamic(bool shouldCollide);
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

    void InvokeContactRemovedEvent(PhysicsBody* other);

    void InvokeContactAddedEvent(
        PhysicsBody* other, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings
    );

    void SetLinearVelocity(const glm::vec3& velocity);
    [[LuaFn]] glm::vec3 GetLinearVelocity();
    [[LuaFn]] void AddForce(const glm::vec3& force);
    [[LuaFn]] void AddImpulse(const glm::vec3& impulse);
    [[LuaFn]] void SetGravityFactor(float f);
    void UpdateGameObject();
    JPH::Body* GetBody() { return body; }
    JPH::Ref<JPH::Shape> GetShapeRef() { return shapeRef; }

    void OnStart() override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() const override;
    void Tick() override;

    void RegisterLuaCallback(PhysicsContactEvent event, GameScript* gameScript, const char* luaCallbackName);

    // set this to true, the physics scene will try to draw this physics body in this frame
    bool debugDrawRequest = false;

    // get registered lua callback events
    const std::vector<PhysicsLuaCallback>& GetContactAddedLuaCallbacks() const { return contactAddedLuaCallbacks; }
    const std::vector<PhysicsLuaCallback>& GetContactRemovedLuaCallbacks() const { return contactRemovedLuaCallbacks; }
    const std::vector<PhysicsLuaCallback>& GetContactPersistedLuaCallbacks() const
    {
        return contactPersistedLuaCallbacks;
    }
    void SetContactAddedLuaCallbacks(const std::vector<PhysicsLuaCallback>& callbacks)
    {
        contactAddedLuaCallbacks = callbacks;
    }
    void SetContactRemovedLuaCallbacks(const std::vector<PhysicsLuaCallback>& callbacks)
    {
        contactRemovedLuaCallbacks = callbacks;
    }
    void SetContactPersistedLuaCallbacks(const std::vector<PhysicsLuaCallback>& callbacks)
    {
        contactPersistedLuaCallbacks = callbacks;
    }

private:
    using ContactAddedEventCallbackType =
        std::function<void(PhysicsBody*, PhysicsBody*, const JPH::ContactManifold&, JPH::ContactSettings&)>;
    using ContactRemovedEventCallbackType = std::function<void(PhysicsBody*, PhysicsBody*)>;

    glm::vec4 bodyScale = {0.5, 0.5, 0.5, 1.0};
    glm::vec4 bodyOffset = {0.0, 0.0, 0.0, 0.0};
    PhysicsLayer layer = PhysicsLayer::Scene;
    float gravityFactor = 0.0f;
    bool isSensor = false;
    bool kinematicGenerateContactPointsWithNonDynamic = false;

    JPH::EMotionType motionType = JPH::EMotionType::Static;
    JPH::Ref<JPH::Shape> shapeRef;
    JPH::Body* body = nullptr;
    PhysicsBodyShapes shapeType = PhysicsBodyShapes::Mesh;

    std::function<bool()> recreateShape = nullptr;
    std::vector<ContactAddedEventCallbackType> contactAddedCallbacks = {};
    std::vector<ContactRemovedEventCallbackType> contactRemovedCallbacks = {};

    std::vector<PhysicsLuaCallback> contactAddedLuaCallbacks = {};
    std::vector<PhysicsLuaCallback> contactRemovedLuaCallbacks = {};
    std::vector<PhysicsLuaCallback> contactPersistedLuaCallbacks = {};

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
