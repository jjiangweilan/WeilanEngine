#pragma once
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Game/Input.hpp"
#include "Engine/Library/Hive.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"

class UI
{
public:
    UI();
    ~UI();

    // this is in game view space
    void SetUICanvasCoordinate(int2 origin, int2 size);
    void DragOverlay();
    void DrawTexture(int2 origin, int2 size, ObjPtr<Texture>& texture, const std::string& name = "");

    void RenderElements(const Gfx::ImageIdentifier* colorImage);

private:
    struct UIElement
    {
        std::string name = "";
        ObjPtr<Texture> texture;
        int2 orgin;
        int2 size;
    };

    std::unique_ptr<Gfx::CommandBuffer> cmd;
    ObjPtr<Texture> whiteTexture;
    plf::hive<UIElement> uiElements;
    int2 canvasOrigin;
    int2 canvasSize;
};
