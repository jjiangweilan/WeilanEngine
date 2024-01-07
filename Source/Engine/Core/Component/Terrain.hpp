#pragma once
#include "Component.hpp"

class Terrain : public Component
{
    DECLARE_OBJECT()
public:
    Terrain();
    Terrain(GameObject* owner);
    ~Terrain() override{};
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
    const std::string& GetName() override;
    std::unique_ptr<Component> Clone(GameObject& owner) override;

    void EnableImple() override;
    void DisableImple() override;
};
