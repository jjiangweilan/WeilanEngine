#include "DrawUI.hpp"
#include "Core/GameObject.hpp"
#include "Core/Component/ImageUI.hpp"

void Draw(UI* ui)
{
    auto imagUIs = ui->GetGameObject()->GetComponentsInChildren<ImageUI>();
}
