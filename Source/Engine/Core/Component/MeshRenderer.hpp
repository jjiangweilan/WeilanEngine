#pragma once

#include "Component.hpp"
#include "Core/Graphics/Material.hpp"
#include "Core/Graphics/Mesh2.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Utils/Structs.hpp"
#include <memory>
namespace Engine
{
class MeshRenderer : public Component
{
    DECLARE_COMPONENT(MeshRenderer);

public:
    MeshRenderer();
    MeshRenderer(GameObject* parent, Mesh2* mesh, Material* material);
    MeshRenderer(GameObject* parent);
    ~MeshRenderer() override{};

    void SetMesh(Mesh2* mesh);
    void SetMaterials(std::span<Material*> materials);
    Mesh2* GetMesh();
    const std::vector<Material*>& GetMaterials();

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
