#pragma once
#include "Component.hpp"

class SceneEnvironment : public Component
{
    DECLARE_OBJECT();

public:
    SceneEnvironment() : Component(nullptr){};
    SceneEnvironment(GameObject* owner) : Component(owner){};
    ~SceneEnvironment() override{};

    void Serialize(Serializer* s) const override{};
    void Deserialize(Serializer* s) override{};
    const std::string& GetName() override;
    std::unique_ptr<Component> Clone(GameObject& owner) override
    {
        return std::make_unique<SceneEnvironment>();
    }

    void EnableImple() override;
    void DisableImple() override;
};
