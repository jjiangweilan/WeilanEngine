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

void PhysicsBody::EnableImple() {}
void PhysicsBody::DisableImple() {}

void PhysicsBody::UpdateBody()
{
    Scene* scene = GetScene();

    if (scene)
    {
        auto& physicsWorld = scene->GetPhysicsScene();
    }
}

bool PhysicsBody::SetShape(JPH::ShapeSettings& shape)
{
    auto result = shape.Create();
    if (!result.IsValid())
    {
        return false;
    }

    shapeRef = result.Get();
}
