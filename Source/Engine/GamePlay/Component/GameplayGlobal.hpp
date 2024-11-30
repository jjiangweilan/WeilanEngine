#pragma once
#include "Core/Component/Component.hpp"

class GameplayGlobal : public Component
{
    DECLARE_OBJECT();

public:
    GameplayGlobal();
    GameplayGlobal(GameObject* gameObject);
    ~GameplayGlobal();

    void Tick() override;

    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;
    void Serialize(Serializer* s) const override;
    void Deserialize(Serializer* s) override;
};
