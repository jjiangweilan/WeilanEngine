#pragma once

#include "Asset/Material.hpp"
#include "Component.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Utils/Structs.hpp"
#include <memory>
namespace Engine
{
class MeshRenderer : public Component
{
    DECLARE_OBJECT();

public:
    MeshRenderer();
    MeshRenderer(GameObject* owner, Mesh* mesh, Material* material);
    MeshRenderer(GameObject* owner);
    ~MeshRenderer() override{};

    void SetMesh(Mesh* mesh);
    void SetMaterials(std::span<Material*> materials);
    Mesh* GetMesh();
    const std::vector<Material*>& GetMaterials();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

private:
    Mesh* mesh = nullptr;
    std::vector<Material*> materials = {};
    AABB aabb;
    // UniPtr<Gfx::ShaderResource>
    // objectShaderResource; // TODO: should be an EDITABLE but we can't directly serialize a ShaderResource

    // we need shader to create a shader resource, but it's not known untill user set one.
    // this is a helper function to create objectShaderResource if it doesn't exist
    void TryCreateObjectShaderResource();
};
} // namespace Engine
