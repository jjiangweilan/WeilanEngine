#include "PhysicsBody.hpp"
#include "Engine/Core/GameLoop.hpp"
#include "Engine/Library/TypeReflection.hpp"
#include "Engine/Runtime/Object/Component/GameScript.hpp"
#include "Engine/Runtime/Object/Component/MeshRenderer.hpp"
#include "Engine/Runtime/Object/GameObject/GameObject.hpp"
#include "Engine/Runtime/System/SceneManager/Scene.hpp"
#include "Jolt/Physics/Collision/Shape/StaticCompoundShape.h"

using namespace JPH;
using namespace JPH::literals;

DEFINE_OBJECT(Component, PhysicsBody, "670E10EF-2532-4977-A356-63C9B07C6F5D");

TYPE_REFLECTION_MEMBER_VARIABLES(
    PhysicsBody,
    TYPE_REFLECTION_MEM(PhysicsBody, bodyScale),
    TYPE_REFLECTION_MEM(PhysicsBody, bodyOffset),
    TYPE_REFLECTION_MEM(PhysicsBody, layer),
    TYPE_REFLECTION_MEM(PhysicsBody, gravityFactor),
    TYPE_REFLECTION_MEM(PhysicsBody, motionType),
    TYPE_REFLECTION_MEM(PhysicsBody, shapeType),
    TYPE_REFLECTION_MEM(PhysicsBody, isSensor),
    TYPE_REFLECTION_MEM(PhysicsBody, kinematicGenerateContactPointsWithNonDynamic)
);
PhysicsBody::PhysicsBody() : Component(nullptr) {}

PhysicsBody::PhysicsBody(GameObject* gameObject) : Component(gameObject) {}

void PhysicsBody::Serialize(Serializer* s) const
{
    Component::Serialize(s);

    s->Serialize("bodyScale", bodyScale);
    s->Serialize("bodyOffset", bodyOffset);
    s->Serialize("layer", static_cast<int>(layer));
    s->Serialize("gravityFactor", gravityFactor);
    s->Serialize("motionType", static_cast<int>(motionType));
    s->Serialize("shapeType", static_cast<int>(shapeType));
    s->Serialize("isSensor", isSensor);
    s->Serialize("kinematicGenerateContactPointsWithNonDynamic", kinematicGenerateContactPointsWithNonDynamic);
    SERIALIZE(s, contactAddedLuaCallbacks);
    SERIALIZE(s, contactRemovedLuaCallbacks);
    SERIALIZE(s, contactPersistedLuaCallbacks);
}
void PhysicsBody::Deserialize(Serializer* s)
{
    Component::Deserialize(s);

    s->Deserialize("bodyScale", bodyScale);
    s->Deserialize("bodyOffset", bodyOffset);
    int layer = 0;
    s->Deserialize("layer", layer);
    this->layer = static_cast<PhysicsObjectLayer>(layer);
    s->Deserialize("gravityFactor", gravityFactor);
    int motionType = 0;
    s->Deserialize("motionType", motionType);
    this->motionType = static_cast<JPH::EMotionType>(motionType);
    int shapeType = 0;
    s->Deserialize("shapeType", shapeType);
    this->shapeType = static_cast<PhysicsBodyShapes>(shapeType);
    s->Deserialize("isSensor", isSensor);
    s->Deserialize("kinematicGenerateContactPointsWithNonDynamic", kinematicGenerateContactPointsWithNonDynamic);
    DESERIALIZE(s, contactAddedLuaCallbacks);
    DESERIALIZE(s, contactRemovedLuaCallbacks);
    DESERIALIZE(s, contactPersistedLuaCallbacks);
}

const std::string& PhysicsBody::GetName() const
{
    static std::string name = "PhysicsBody";
    return name;
}

std::unique_ptr<Component> PhysicsBody::Clone(GameObject& owner)
{
    auto clone = std::make_unique<PhysicsBody>(&owner);
    clone->enabled = enabled;
    clone->bodyScale = bodyScale;
    clone->bodyOffset = bodyOffset;
    clone->layer = layer;
    clone->gravityFactor = gravityFactor;
    clone->motionType = motionType;
    clone->shapeType = shapeType;
    clone->isSensor = isSensor;
    clone->kinematicGenerateContactPointsWithNonDynamic = kinematicGenerateContactPointsWithNonDynamic;

    // we don't know which scene the body will be attached to, so we don't Init here
    clone->body = nullptr;
    clone->shapeRef = nullptr;

    return clone;
}

void PhysicsBody::OnEnable()
{
    Scene* scene = GetScene();
    if (scene == nullptr)
        return;

    if (body == nullptr)
    {
        Init();
    }
    else
    {
        auto& physicsScene = scene->GetPhysicsScene();
        physicsScene.AddPhysicsBody(*this);
        physicsScene.GetBodyInterface().ActivateBody(body->GetID());
    }
}

void PhysicsBody::OnDisable()
{
    Scene* scene = GetScene();
    if (scene == nullptr)
        return;

    auto& physicsScene = scene->GetPhysicsScene();
    if (body)
        physicsScene.GetBodyInterface().DeactivateBody(body->GetID());
    physicsScene.RemovePhysicsBody(*this);
}

void PhysicsBody::SetLayer(PhysicsObjectLayer layer)
{
    this->layer = layer;

    if (auto interface = GetBodyInterface())
    {
        interface->SetObjectLayer(body->GetID(), static_cast<JPH::ObjectLayer>(layer));
    }
}

JPH::BodyInterface* PhysicsBody::GetBodyInterface()
{
    Scene* scene = GetScene();
    if (scene == nullptr)
        return nullptr;

    auto& physicsWorld = scene->GetPhysicsScene();
    auto& bodyInterface = physicsWorld.GetBodyInterface();
    return &bodyInterface;
}

bool PhysicsBody::SetShape(JPH::ShapeSettings& shape)
{
    auto result = shape.Create();
    if (result.IsValid())
    {
        shapeRef = result.Get();
        if (body)
        {
            if (result.IsValid())
            {
                if (auto interface = GetBodyInterface())
                {
                    interface->SetShape(
                        body->GetID(),
                        result.Get(),
                        false,
                        (static_cast<int>(layer) & static_cast<int>(PhysicsObjectLayers::Dynamic)) == 1
                            ? EActivation::Activate
                            : EActivation::DontActivate
                    );
                }
            }
        }
        else
        {
            glm::vec3 position = gameObject->GetPosition();
            glm::quat rot = gameObject->GetRotation();
            BodyCreationSettings bodyCreationSettings(
                shapeRef,
                RVec3{position.x + bodyOffset.x, position.y + bodyOffset.y, position.z + bodyOffset.z},
                Quat{rot.x, rot.y, rot.z, rot.w},
                motionType,
                static_cast<ObjectLayer>(layer)
            );
            bodyCreationSettings.mIsSensor = isSensor;
            // bodyCreationSettings.mAllowDynamicOrKinematic = motionType != EMotionType::Static;
            bodyCreationSettings.mMotionType = motionType;
            bodyCreationSettings.mCollideKinematicVsNonDynamic = kinematicGenerateContactPointsWithNonDynamic;
            if (shapeType == PhysicsBodyShapes::Mesh)
            {
                bodyCreationSettings.mOverrideMassProperties = EOverrideMassProperties::MassAndInertiaProvided;
                bodyCreationSettings.mMassPropertiesOverride = {1.0, JPH::Mat44::sZero()};
            }

            auto& physicsWorld = GetScene()->GetPhysicsScene();
            auto& bodyInterface = physicsWorld.GetBodyInterface();

            if (body != nullptr)
            {
                bodyInterface.RemoveBody(body->GetID());
                bodyInterface.DestroyBody(body->GetID());
            }

            body = bodyInterface.CreateBody(bodyCreationSettings);
            bodyInterface.AddBody(body->GetID(), EActivation::DontActivate);
            SetGravityFactor(gravityFactor);
            body->SetUserData(reinterpret_cast<std::intptr_t>(this));

            auto& physicsScene = GetScene()->GetPhysicsScene();
            physicsScene.AddPhysicsBody(*this);
            physicsScene.GetBodyInterface().ActivateBody(body->GetID());
        }
    }
    return false;
}

PhysicsBody::~PhysicsBody()
{
    Scene* scene = GetScene();
    if (scene == nullptr)
        return;

    if (body)
    {
        auto& physicsWorld = scene->GetPhysicsScene();
        auto& bodyInterface = physicsWorld.GetBodyInterface();
        bodyInterface.RemoveBody(body->GetID());
        bodyInterface.DestroyBody(body->GetID());
    }
}

void PhysicsBody::Init()
{
    Scene* scene = GetScene();
    if (scene == nullptr)
        return;

    // create from a mesh renderer if we can
    switch (shapeType)
    {
        case PhysicsBodyShapes::Box: SetAsBox(); break;
        case PhysicsBodyShapes::Mesh:
            {
                if (!SetAsMeshRenderer())
                {
                    SetAsBox();
                }
                break;
            }
        case PhysicsBodyShapes::Sphere: SetAsSphere(); break;
        case PhysicsBodyShapes::Capsule: SetAsCapsule(); break;
    }
}

void PhysicsBody::SetGravityFactor(float f)
{
    gravityFactor = f;
    if (auto i = GetBodyInterface())
    {
        i->SetGravityFactor(body->GetID(), f);
    }
}

void PhysicsBody::UpdateGameObject()
{
    if (body && motionType == JPH::EMotionType::Dynamic)
    {
        auto newPos = body->GetPosition();
        gameObject->SetPosition(
            {newPos.GetX() - bodyOffset.x, newPos.GetY() - bodyOffset.y, newPos.GetZ() - bodyOffset.z}
        );

        auto newRot = body->GetRotation();
        gameObject->SetRotation({newRot.GetW(), newRot.GetX(), newRot.GetY(), newRot.GetZ()});
    }
}

bool PhysicsBody::SetAsSphere()
{
    if (bodyScale.x <= 0)
        return false;

    recreateShape = [this]()
    {
        float sphereSize = gameObject->GetScale().x * bodyScale.x;
        JPH::SphereShapeSettings s(sphereSize);
        SetShape(s);
        UpdateBodyPositionAndRotation();
        return true;
    };

    return recreateShape();
}

bool PhysicsBody::SetAsBox()
{
    if (glm::any(glm::lessThanEqual(glm::vec3(bodyScale), glm::vec3(0))))
        return false;

    recreateShape = [this]()
    {
        glm::vec3 boxSize = glm::vec3(bodyScale) * gameObject->GetScale();
        JPH::BoxShapeSettings s({boxSize.x, boxSize.y, boxSize.z});
        SetShape(s);
        UpdateBodyPositionAndRotation();
        return true;
    };

    return recreateShape();
}

bool PhysicsBody::SetAsCapsule()
{
    recreateShape = [this]()
    {
        auto scale = gameObject->GetScale();
        JPH::CapsuleShapeSettings s(bodyScale.x * scale.x, bodyScale.y * scale.y);
        SetShape(s);
        UpdateBodyPositionAndRotation();
        return true;
    };

    return recreateShape();
}

void PhysicsBody::SetMotionType(JPH::EMotionType motionType)
{
    if (GameLoop::IsPlaying())
    {
        if (this->motionType == JPH::EMotionType::Static && motionType != JPH::EMotionType::Static)
        {
            spdlog::warn("Changing motion type from Static to non-Static at runtime is prohibited."); // TODO: we can add mAllowDynamicOrKinematic(Jolt physics)
            return;
        }
    }

    this->motionType = motionType;
    if (auto interface = GetBodyInterface())
    {
        interface->SetMotionType(body->GetID(), motionType, EActivation::DontActivate);
    }
}

void PhysicsBody::OnStart() {}

void PhysicsBody::TransformChanged()
{
    if (auto i = GetBodyInterface())
    {
        if (body)
        {
            auto pos = gameObject->GetPosition();
            auto rot = gameObject->GetRotation();
            i->SetPositionAndRotation(
                body->GetID(),
                {pos.x + bodyOffset.x, pos.y + bodyOffset.y, pos.z + bodyOffset.z},
                {rot.x, rot.y, rot.z, rot.w},
                EActivation::DontActivate
            );
        }
    }
}

void PhysicsBody::SetLinearVelocity(const glm::vec3& velocity)
{
    if (auto i = GetBodyInterface())
    {
        i->SetLinearVelocity(body->GetID(), {velocity.x, velocity.y, velocity.z});
    }
}

void PhysicsBody::AddForce(const glm::vec3& force)
{
    body->AddForce({force.x, force.y, force.z});
}

void PhysicsBody::Tick()
{
    if (motionType == JPH::EMotionType::Kinematic && body)
    {
        auto pos = gameObject->GetPosition();

        JPH::Vec3 worldPos = {pos.x + bodyOffset.x, pos.y + bodyOffset.y, pos.z + bodyOffset.z};
        auto rot = gameObject->GetRotation();
        GetBodyInterface()
            ->SetPositionAndRotation(body->GetID(), worldPos, {rot.w, rot.x, rot.y, rot.z}, JPH::EActivation::Activate);
    }
}

glm::vec3 PhysicsBody::GetLinearVelocity()
{
    auto v = body->GetLinearVelocity();
    return {v.GetX(), v.GetY(), v.GetZ()};
}

void PhysicsBody::AddImpulse(const glm::vec3& impulse)
{
    body->AddImpulse({impulse.x, impulse.y, impulse.z});
}

PhysicsScene* PhysicsBody::GetPhysicsScene()
{
    if (auto scene = GetScene())
    {
        return &scene->GetPhysicsScene();
    }
    return nullptr;
}

void PhysicsBody::UpdateBodyPositionAndRotation()
{
    if (auto i = GetBodyInterface())
    {
        if (body)
        {
            auto pos = gameObject->GetPosition();
            auto rot = gameObject->GetRotation();
            i->SetPositionAndRotation(
                body->GetID(),
                {pos.x + bodyOffset.x, pos.y + bodyOffset.y, pos.z + bodyOffset.z},
                {rot.x, rot.y, rot.z, rot.w},
                EActivation::DontActivate
            );
        }
    }
}

bool PhysicsBody::SetAsMeshRenderer()
{
    recreateShape = [this]()
    {
        JPH::Array<JPH::Triangle> triangles;
        bool createFromMeshRenderer = GenerateTrianglesFromMeshRenderer(triangles);

        if (createFromMeshRenderer)
        {
            JPH::MeshShapeSettings meshShapeSettings(triangles);
            if (motionType == JPH::EMotionType::Dynamic)
            {
                spdlog::warn("dynamic mesh shape is not supported, expect errors");
            }
            SetShape(meshShapeSettings);

            UpdateBodyPositionAndRotation();
        }

        return createFromMeshRenderer;
    };

    return recreateShape();
}

bool PhysicsBody::GenerateTrianglesFromMeshRenderer(JPH::Array<JPH::Triangle>& triangles)
{
    bool createFromMeshRenderer = false;
    auto meshRenderers = gameObject->GetComponentsInChildren<MeshRenderer>();
    for (auto meshRenderer : meshRenderers)
    {
        auto mesh = meshRenderer ? meshRenderer->GetMesh() : nullptr;
        auto scale = GetGameObject()->GetScale();
        if (mesh)
        {
            auto& submeshes = mesh->GetSubmeshes();
            for (auto& submesh : submeshes)
            {
                auto& indices = submesh.GetIndices();
                auto& vertexPositions = submesh.GetPositions();
                for (int i = 0; i < indices.size(); i += 3)
                {
                    const int i0 = indices[i];
                    const int i1 = indices[i + 1];
                    const int i2 = indices[i + 2];
                    const JPH::Vec3 v0 =
                        {vertexPositions[i0].x * scale.x, vertexPositions[i0].y * scale.y, vertexPositions[i0].z * scale.z};
                    const JPH::Vec3 v1 =
                        {vertexPositions[i1].x * scale.x, vertexPositions[i1].y * scale.y, vertexPositions[i1].z * scale.z};
                    const JPH::Vec3 v2 =
                        {vertexPositions[i2].x * scale.x, vertexPositions[i2].y * scale.y, vertexPositions[i2].z * scale.z};
                    triangles.push_back({v0, v1, v2});
                }
            }
            createFromMeshRenderer = !triangles.empty();
        }
    }

    return createFromMeshRenderer;
}

void PhysicsBody::SetShape(PhysicsBodyShapes shape)
{
    this->shapeType = shape;

    if (GetScene() != nullptr && GetBody() != nullptr)
    {
        switch (shape)
        {
            case PhysicsBodyShapes::Box: SetAsBox(); break;
            case PhysicsBodyShapes::Sphere: SetAsSphere(); break;
            case PhysicsBodyShapes::Mesh: SetAsMeshRenderer(); break;
            case PhysicsBodyShapes::Capsule: SetAsCapsule(); break;
            case PhysicsBodyShapes::Compound: SetAsCompound(); break;
        }
    }
}

bool PhysicsBody::SetAsCompound()
{
    recreateShape = [this]()
    {
        // Define the shapes that will be part of the compound shape
        JPH::Array<JPH::Ref<JPH::Shape>> shapes;

        // Example: Adding a box and a sphere to the compound shape
        glm::vec3 boxSize = glm::vec3(bodyScale) * gameObject->GetScale();
        JPH::BoxShapeSettings boxSettings({boxSize.x, boxSize.y, boxSize.z});
        auto boxShape = boxSettings.Create().Get();
        shapes.push_back(boxShape);

        float sphereSize = gameObject->GetScale().x * bodyScale.x;
        JPH::SphereShapeSettings sphereSettings(sphereSize);
        auto sphereShape = sphereSettings.Create().Get();
        shapes.push_back(sphereShape);

        // Create the compound shape settings
        JPH::StaticCompoundShapeSettings compoundSettings;
        SetShape(compoundSettings);

        UpdateBodyPositionAndRotation();
        return true;
    };

    return recreateShape();
}

void PhysicsBody::SetSensor(bool isSensor)
{
    bool oldVal = this->isSensor;
    this->isSensor = isSensor;

    if (oldVal != this->isSensor)
    {
        if (body)
            body->SetIsSensor(this->isSensor);
    }
}

void PhysicsBody::RegisterLuaCallback(PhysicsContactEvent event, GameScript* gameScript, const char* luaCallbackName)
{

    std::vector<PhysicsLuaCallback>* callbacks = nullptr;
    switch (event)
    {
        case PhysicsContactEvent::Added: callbacks = &contactAddedLuaCallbacks;
        case PhysicsContactEvent::Removed: callbacks = &contactRemovedLuaCallbacks;
        case PhysicsContactEvent::Persisted: callbacks = &contactPersistedLuaCallbacks;
    }

    if (callbacks == nullptr)
        return;

    PhysicsLuaCallback cb{};
    cb.gameScript = gameScript;
    cb.callback = luaCallbackName;

    callbacks->push_back(cb);
}

void PhysicsBody::InvokeContactAddedEvent(
    PhysicsBody* other, const JPH::ContactManifold& manifold, JPH::ContactSettings& settings
)
{
    for (auto& f : contactAddedCallbacks)
    {
        f(this, other, manifold, settings);
    }

    for (auto& c : contactAddedLuaCallbacks)
    {
        if (c.gameScript)
        {
            c.gameScript->CallLua(c.callback.c_str());
        }
    }
}

void PhysicsBody::InvokeContactRemovedEvent(PhysicsBody* other)
{
    for (auto& f : contactRemovedCallbacks)
    {
        f(this, other);
    }

    for (auto& c : contactRemovedLuaCallbacks)
    {
        if (c.gameScript)
        {
            c.gameScript->CallLua(c.callback.c_str());
        }
    }
}

void PhysicsBody::SetKinematicCollideWithNonDynamic(bool shouldCollide)
{
    kinematicGenerateContactPointsWithNonDynamic = shouldCollide;

    if (body && body->GetCollideKinematicVsNonDynamic() != kinematicGenerateContactPointsWithNonDynamic)
    {
        body->SetCollideKinematicVsNonDynamic(kinematicGenerateContactPointsWithNonDynamic);
    }
}

void PhysicsLuaCallback::Serialize(Serializer* s) const
{
    SERIALIZE(s, gameScript);
    SERIALIZE(s, callback);
}

void PhysicsLuaCallback::Deserialize(Serializer* s)
{
    DESERIALIZE(s, gameScript);
    DESERIALIZE(s, callback);
}
