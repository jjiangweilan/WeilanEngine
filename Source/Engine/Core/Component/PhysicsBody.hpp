#pragma once

#include "Asset/Material.hpp"
#include "Component.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Utils/Structs.hpp"
#include <memory>
class RenderingScene;
class PhysicsBody : public Component
{
    DECLARE_OBJECT();

public:
    PhysicsBody();
    PhysicsBody(GameObject* owner);
    ~PhysicsBody() override{};

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

private:
    void EnableImple() override{};
    void DisableImple() override{};
};
