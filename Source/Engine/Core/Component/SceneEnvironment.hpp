#pragma once
#include "Component.hpp"

class Texture;
class SceneEnvironment : public Component
{
    DECLARE_OBJECT();

public:
    SceneEnvironment() : Component(nullptr){};
    SceneEnvironment(GameObject* owner) : Component(owner){};
    ~SceneEnvironment() override{};

    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    const std::string& GetName() override;
    std::unique_ptr<Component> Clone(GameObject& owner) override
    {
        auto clone = std::make_unique<SceneEnvironment>();
        clone->diffuseCube = diffuseCube;
        clone->specularCube = specularCube;
        return clone;
    }

    void SetDiffuseCube(Texture* diffuseCube);
    void SetSpecularCube(Texture* specularCube);

    Texture* GetDiffuseCube()
    {
        return diffuseCube;
    }

    Texture* GetSpecularCube()
    {
        return specularCube;
    }

    void EnableImple() override;
    void DisableImple() override;

private:
    Texture* diffuseCube = nullptr;
    Texture* specularCube = nullptr;
};
