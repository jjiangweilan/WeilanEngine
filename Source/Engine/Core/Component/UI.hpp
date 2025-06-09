#pragma once
#include "Component.hpp"

class UI : public Component
{
    DECLARE_OBJECT();

public:
    UI();
    UI(GameObject* gameObject);
    ~UI();

    void OnEnable() override;
    void OnDisable() override;

    const std::string& GetName() override;
};
