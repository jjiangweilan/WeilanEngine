#pragma once

#include "Asset/Material.hpp"
#include "Component.hpp"
#include "Core/Graphics/Mesh.hpp"
#include "GfxDriver/ShaderResource.hpp"
#include "Utils/Structs.hpp"
#include <memory>
class RenderingScene;
class Cloud : public Component
{
    DECLARE_OBJECT();

public:
    Cloud();
    Cloud(GameObject* owner);
    ~Cloud() override{};

    void SetMesh(Mesh* mesh);
    Mesh* GetMesh();

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

private:
    Mesh* mesh = nullptr;
    AABB aabb;

    void AddToRenderingScene();
    void RemoveFromRenderingScene();

    void EnableImple() override;
    void DisableImple() override;
};
