#pragma once
#include "Core/Component/Component.hpp"

struct FoliagePatch : public Component
{
    DECLARE_OBJECT();

public:
    FoliagePatch() : Component(nullptr) {}
    FoliagePatch(GameObject* gameObject) : Component(gameObject) {};
    ~FoliagePatch(){};
    std::unique_ptr<Component> Clone(GameObject& owner) override;
    const std::string& GetName() override;

private:
};
