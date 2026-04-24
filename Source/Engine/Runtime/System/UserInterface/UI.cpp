#include "UI.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include <spdlog/spdlog.h>

UI::UI()
{
    whiteTexture = &EngineInternalResources::GetWhiteTexture();
    cmd = GetGfxDriver()->CreateCommandBuffer();
}

UI::~UI()
{
}

void UI::DragOverlay()
{
    if (Input::IsMouseButtonDown(MouseButton::Left))
    {
        DrawTexture({10, 10}, {128, 128}, whiteTexture);
        auto useDelta = Input::GetMouseDelta();
    }
}

void UI::DrawTexture(int2 origin, int2 size, ObjPtr<Texture>& texture, const std::string& name)
{
    UIElement element;
    element.name = name;
    element.orgin = origin;
    element.size = size;
    element.texture = texture;

    uiElements.insert(std::move(element));
}

void UI::RenderElements(const Gfx::ImageIdentifier* colorImage)
{
    Gfx::RenderAttachment colorAttachment[1] =
        {
            {
                .image = *colorImage,
                .loadOp = Gfx::AttachmentLoadOperation::Load,
                .storeOp = Gfx::AttachmentStoreOperation::Store,
            }
        };
    Gfx::ClearValue clearValues[1] = {{}};

    cmd->BeginRenderPass(colorAttachment, clearValues);

    for (auto& element : uiElements)
    {
        Gfx::Viewport vp;
        vp.x = static_cast<float>(canvasOrigin.x + element.orgin.x);
        vp.y = static_cast<float>(canvasOrigin.y + element.orgin.y);
        vp.width = static_cast<float>(element.size.x);
        vp.height = static_cast<float>(element.size.y);
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        cmd->SetViewport(vp);

        ObjPtr<Texture> tex = element.texture ? element.texture : whiteTexture;
    }

    cmd->EndRenderPass();

    uiElements.clear();
}
