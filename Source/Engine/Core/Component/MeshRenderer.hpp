#pragma once

#include "Asset/Material.hpp"
#include "Component.hpp"
#include "Core/Graphics/Mesh2.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Utils/Structs.hpp"
#include <memory>
namespace Engine
{
class MeshRenderer : public Component
{
public:
    MeshRenderer();
    MeshRenderer(GameObject* parent, Mesh2* mesh, Material* material);
    MeshRenderer(GameObject* parent);
    ~MeshRenderer() override{};

    void SetMesh(Mesh2* mesh);
    void SetMaterials(std::span<Material*> materials);
    Mesh2* GetMesh();
    const std::vector<Material*>& GetMaterials();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;

private:
    Mesh2* mesh = nullptr;
    std::vector<Material*> materials = {};
    AABB aabb;
    // UniPtr<Gfx::ShaderResource>
    // objectShaderResource; // TODO: should be an EDITABLE but we can't directly serialize a ShaderResource

    // we need shader to create a shader resource, but it's not known untill user set one.
    // this is a helper function to create objectShaderResource if it doesn't exist
    void TryCreateObjectShaderResource();
};
} // namespace Engine
