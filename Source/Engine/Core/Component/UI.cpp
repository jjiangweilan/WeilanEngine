#include "UI.hpp"
#include "Core/Scene/Scene.hpp"

DEFINE_OBJECT(UI, "124A6D33-16F2-4B84-963D-067FA22BCD34");

UI::UI() : Component(nullptr) {}
UI::UI(GameObject* gameObject) : Component(gameObject) {};

const std::string& UI::GetName()
{
    static std::string name = "UI";
    return name;
}

void UI::OnEnable()
{
    GetScene()->GetRenderingScene().AddRenderObject(*this);
}

void UI::OnDisable()
{
    GetScene()->GetRenderingScene().RemoveRenderObject(*this);
}
