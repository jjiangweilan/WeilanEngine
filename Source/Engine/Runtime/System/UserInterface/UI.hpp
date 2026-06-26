#pragma once
#include "Engine/Driver/GfxDriver/CommandBuffer.hpp"
#include "Engine/Game/Input.hpp"
#include "Engine/Library/Hive.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include <RmlUi/Core.h>
#include <SDL_events.h>
#include <string_view>

namespace Rml
{
class Context;
class ElementDocument;
class SystemInterface;
} // namespace Rml

class RmlUiRenderer;

class UI
{
public:
    UI();
    ~UI();

    // this is in game view space
    void SetUICanvasCoordinate(int2 origin, int2 size);
    Rml::ElementDocument* LoadDocument(std::string_view path);
    bool LoadFontFace(std::string_view path, bool fallbackFace = false);
    Rml::Context* GetRmlContext();
    static void ProcessSDLEvent(const SDL_Event& event);
    void DragOverlay();
    void DrawTexture(int2 origin, int2 size, ObjPtr<Texture>& texture, const std::string& name = "");
    void InitLuaBinding();
    void SetRmlDebuggerVisible(bool visible);
    void ToggleRmlDebugger();
    bool IsRmlDebuggerVisible() const;

    void RenderElements(const Gfx::ImageIdentifier* colorImage);
    void Init();
    void Update();
    void Destroy();

    static UI& Instance();

private:
    struct UIElement
    {
        std::string name = "";
        ObjPtr<Texture> texture;
        int2 orgin;
        int2 size;
    };

    std::unique_ptr<Gfx::CommandBuffer> cmd;
    std::unique_ptr<Rml::SystemInterface> rmlSystem;
    std::unique_ptr<RmlUiRenderer> rmlRenderer;
    Rml::Context* rmlContext = nullptr;
    bool rmlInitialized = false;
    bool rmlDebuggerInitialized = false;
    bool rmlDebuggerVisible = false;
    ObjPtr<Texture> whiteTexture;
    plf::hive<UIElement> uiElements;
    int2 canvasOrigin = {0, 0};
    int2 canvasSize = {1, 1};
};
