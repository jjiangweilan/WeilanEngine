#include "DrawUI.hpp"
#include "Core/Component/ImageUI.hpp"
#include "Core/FrameContext.hpp"
#include "Core/GameObject.hpp"
#include "GfxDriver/GfxDriver.hpp"

void DrawUI::Draw(UI* ui)
{
    auto imageUIs = ui->GetGameObject()->GetComponentsInChildren<ImageUI>();

    auto cmd = GetGfxDriver()->CreateCommandBuffer();

    auto& tempAllocator = GetFrameContext().GetTempAllocator();
    void* tempBuffer = tempAllocator.Allocate(perFrameBufferSize, "DrawUI");
    for (auto image : imageUIs)
    {
    }

    GetGfxDriver()->ExecuteCommandBuffer(*cmd);
}
