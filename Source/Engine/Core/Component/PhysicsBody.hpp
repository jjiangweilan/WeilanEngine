#pragma once

#include "Asset/Material.hpp"
#include "Component.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Utils/Structs.hpp"
#include <memory>

// clang-format off
#include <Jolt/Jolt.h>
// clang-format on
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
class RenderingScene;
class PhysicsBody : public Component
{
    DECLARE_OBJECT();

public:
    PhysicsBody();
    PhysicsBody(GameObject* owner);
    ~PhysicsBody() override{};

    bool SetShape(JPH::ShapeSettings& shape);
    void UpdateBody();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

private:
    JPH::ShapeRefC shapeRef;
    JPH::Body* body;

    void EnableImple() override;
    void DisableImple() override;
};
