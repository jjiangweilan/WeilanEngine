#include "PhysicsBody.hpp"
#include "Core/GameObject.hpp"
#include "Core/Scene/Scene.hpp"

DEFINE_OBJECT(PhysicsBody, "670E10EF-2532-4977-A356-63C9B07C6F5D");
PhysicsBody::PhysicsBody() : Component(nullptr) {}

PhysicsBody::PhysicsBody(GameObject* gameObject) : Component(gameObject) {}

void PhysicsBody::Serialize(Serializer* s) const
{
    Component::Serialize(s);
}
void PhysicsBody::Deserialize(Serializer* s)
{
    Component::Deserialize(s);
}

const std::string& PhysicsBody::GetName()
{
    static std::string name = "PhysicsBody";
    return name;
}

std::unique_ptr<Component> PhysicsBody::Clone(GameObject& owner)
{
    auto clone = std::make_unique<PhysicsBody>(&owner);

    return clone;
}

void PhysicsBody::EnableImple()
{
    Scene* scene = GetScene();
    if (scene == nullptr)
        return;

    if (body == nullptr)
    {
        Init();
    }

    auto& physicsScene = scene->GetPhysicsScene();
    physicsScene.AddPhysicsBody(*this);
    physicsScene.GetBodyInterface().ActivateBody(body->GetID());
}
void PhysicsBody::DisableImple()
{

    Scene* scene = GetScene();
    if (scene == nullptr)
        return;

    auto& physicsScene = scene->GetPhysicsScene();
    physicsScene.RemovePhysicsBody(*this);
    physicsScene.GetBodyInterface().DeactivateBody(body->GetID());
}

void PhysicsBody::SetLayer(PhysicsLayer layer)
{
    if (auto interface = GetBodyInterface())
    {
        this->layer = layer;
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
    if (body)
    {
        auto result = shape.Create();
        if (result.IsValid())
        {
            if (auto interface = GetBodyInterface())
            {
                shapeRef->Release();
                shapeRef = result.Get();
                interface->SetShape(
                    body->GetID(),
                    result.Get(),
                    true,
                    (static_cast<int>(layer) & static_cast<int>(PhysicsLayer::MOVING)) == 1 ? EActivation::Activate
                                                                                            : EActivation::DontActivate
                );
            }
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
        shapeRef->Release();
    }
}

void PhysicsBody::Init()
{
    JPH::BoxShapeSettings shape(Vec3{0.5f, 0.5f, 0.5f});
    Scene* scene = GetScene();
    if (scene == nullptr)
        return;

    auto result = shape.Create();
    if (!result.IsValid())
    {
        return;
    }
    shapeRef = result.Get();

    glm::vec3 position = gameObject->GetPosition();
    glm::quat rot = gameObject->GetRotation();
    BodyCreationSettings bodyCreationSettings(
        shapeRef,
        RVec3{position.x, position.y, position.z},
        Quat{rot.x, rot.y, rot.z, rot.w},
        motionType,
        static_cast<ObjectLayer>(layer)
    );

    // wasting space, maybe split class to static(Collider) and dynamic(RigidBody)? or a body creation is needed if set
    // this to false
    bodyCreationSettings.mAllowDynamicOrKinematic = true;

    auto& physicsWorld = scene->GetPhysicsScene();
    auto& bodyInterface = physicsWorld.GetBodyInterface();

    if (body != nullptr)
    {
        bodyInterface.RemoveBody(body->GetID());
        bodyInterface.DestroyBody(body->GetID());
    }

    body = bodyInterface.CreateBody(bodyCreationSettings);
    bodyInterface.AddBody(body->GetID(), EActivation::DontActivate);
    SetGravityFactory(0);
}

void PhysicsBody::SetGravityFactory(float f)
{
    gravityFactor = f;
    if (auto i = GetBodyInterface())
    {
        i->SetGravityFactor(body->GetID(), f);
        if (f != 0)
        {
            i->ActivateBody(body->GetID());
        }
    }
}

void PhysicsBody::UpdateGameObject()
{
    if (body)
    {
        auto newPos = body->GetPosition();
        gameObject->SetPosition({newPos.GetX(), newPos.GetY(), newPos.GetZ()});

        auto newRot = body->GetRotation();
        gameObject->SetRotation({newRot.GetW(), newRot.GetX(), newRot.GetY(), newRot.GetZ()});
    }
}

void PhysicsBody::SetAsSphere(float radius)
{
    if (radius <= 0)
        return;
    JPH::SphereShapeSettings s(radius);
    bodyScale.x = radius;
    SetShape(s);
}

void PhysicsBody::SetAsBox(glm::vec3 halfExtent)
{
    if (glm::any(glm::lessThanEqual(halfExtent, glm::vec3(0))))
        return;
    JPH::BoxShapeSettings s({halfExtent.x, halfExtent.y, halfExtent.z});
    bodyScale = glm::vec4(halfExtent, bodyScale.w);
    SetShape(s);
}

void PhysicsBody::SetMotionType(JPH::EMotionType motionType)
{
    this->motionType = motionType;
    if (auto interface = GetBodyInterface())
    {
        interface->SetMotionType(body->GetID(), motionType, EActivation::DontActivate);
    }
}
