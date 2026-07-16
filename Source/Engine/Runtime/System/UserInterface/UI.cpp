#include "UI.hpp"
#include "Engine/Core/DelayDestroy.hpp"
#include "Engine/MiddleLayer/EngineInternalResources.hpp"
#include "Engine/MiddleLayer/SystemInfo.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/Runtime/System/ScriptingBackend/LuaBackend.hpp"
#include "RmlUiRenderer.hpp"
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/FileInterface.h>
#include <RmlUi/Core/Input.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Debugger.h>
#include <RmlUi/Lua.h>
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <spdlog/spdlog.h>

namespace
{
UI* activeUI = nullptr;

std::filesystem::path GetProjectRoot()
{
    if (auto assetDatabase = AssetDatabase::Singleton())
    {
        return assetDatabase->GetProjectRoot();
    }
    return {};
}

std::filesystem::path ResolveProjectPath(const std::filesystem::path& path)
{
    if (path.is_absolute())
    {
        return path.lexically_normal();
    }

    const std::filesystem::path projectRoot = GetProjectRoot();
    if (projectRoot.empty())
    {
        return path.lexically_normal();
    }

    return (projectRoot / path).lexically_normal();
}

class WeilanRmlUiFileInterface : public Rml::FileInterface
{
public:
    Rml::FileHandle Open(const Rml::String& path) override
    {
        const std::filesystem::path resolvedPath = ResolveProjectPath(std::filesystem::path(path));
        return reinterpret_cast<Rml::FileHandle>(std::fopen(resolvedPath.string().c_str(), "rb"));
    }

    void Close(Rml::FileHandle file) override
    {
        std::fclose(reinterpret_cast<std::FILE*>(file));
    }

    size_t Read(void* buffer, size_t size, Rml::FileHandle file) override
    {
        return std::fread(buffer, 1, size, reinterpret_cast<std::FILE*>(file));
    }

    bool Seek(Rml::FileHandle file, long offset, int origin) override
    {
        return std::fseek(reinterpret_cast<std::FILE*>(file), offset, origin) == 0;
    }

    size_t Tell(Rml::FileHandle file) override
    {
        return static_cast<size_t>(std::ftell(reinterpret_cast<std::FILE*>(file)));
    }
};

std::filesystem::path GetDocumentBasePath(const Rml::String& documentPath)
{
    std::filesystem::path basePath(documentPath);
    if (basePath.has_extension())
    {
        return basePath.parent_path();
    }
    return basePath;
}

int GetRmlKeyModifierState()
{
    SDL_Keymod sdlMods = SDL_GetModState();
    int modifiers = 0;
    if (sdlMods & KMOD_CTRL)
        modifiers |= Rml::Input::KM_CTRL;
    if (sdlMods & KMOD_SHIFT)
        modifiers |= Rml::Input::KM_SHIFT;
    if (sdlMods & KMOD_ALT)
        modifiers |= Rml::Input::KM_ALT;
    if (sdlMods & KMOD_GUI)
        modifiers |= Rml::Input::KM_META;
    if (sdlMods & KMOD_NUM)
        modifiers |= Rml::Input::KM_NUMLOCK;
    if (sdlMods & KMOD_CAPS)
        modifiers |= Rml::Input::KM_CAPSLOCK;
    return modifiers;
}

Rml::Input::KeyIdentifier ConvertSDLKey(SDL_Keycode key)
{
    if (key >= SDLK_a && key <= SDLK_z)
        return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_A + (key - SDLK_a));
    if (key >= SDLK_0 && key <= SDLK_9)
        return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_0 + (key - SDLK_0));
    if (key >= SDLK_F1 && key <= SDLK_F12)
        return static_cast<Rml::Input::KeyIdentifier>(Rml::Input::KI_F1 + (key - SDLK_F1));

    switch (key)
    {
        case SDLK_UNKNOWN: return Rml::Input::KI_UNKNOWN;
        case SDLK_ESCAPE: return Rml::Input::KI_ESCAPE;
        case SDLK_SPACE: return Rml::Input::KI_SPACE;
        case SDLK_SEMICOLON: return Rml::Input::KI_OEM_1;
        case SDLK_PLUS: return Rml::Input::KI_OEM_PLUS;
        case SDLK_COMMA: return Rml::Input::KI_OEM_COMMA;
        case SDLK_MINUS: return Rml::Input::KI_OEM_MINUS;
        case SDLK_PERIOD: return Rml::Input::KI_OEM_PERIOD;
        case SDLK_SLASH: return Rml::Input::KI_OEM_2;
        case SDLK_BACKQUOTE: return Rml::Input::KI_OEM_3;
        case SDLK_LEFTBRACKET: return Rml::Input::KI_OEM_4;
        case SDLK_BACKSLASH: return Rml::Input::KI_OEM_5;
        case SDLK_RIGHTBRACKET: return Rml::Input::KI_OEM_6;
        case SDLK_QUOTEDBL: return Rml::Input::KI_OEM_7;
        case SDLK_KP_0: return Rml::Input::KI_NUMPAD0;
        case SDLK_KP_1: return Rml::Input::KI_NUMPAD1;
        case SDLK_KP_2: return Rml::Input::KI_NUMPAD2;
        case SDLK_KP_3: return Rml::Input::KI_NUMPAD3;
        case SDLK_KP_4: return Rml::Input::KI_NUMPAD4;
        case SDLK_KP_5: return Rml::Input::KI_NUMPAD5;
        case SDLK_KP_6: return Rml::Input::KI_NUMPAD6;
        case SDLK_KP_7: return Rml::Input::KI_NUMPAD7;
        case SDLK_KP_8: return Rml::Input::KI_NUMPAD8;
        case SDLK_KP_9: return Rml::Input::KI_NUMPAD9;
        case SDLK_KP_ENTER: return Rml::Input::KI_NUMPADENTER;
        case SDLK_KP_MULTIPLY: return Rml::Input::KI_MULTIPLY;
        case SDLK_KP_PLUS: return Rml::Input::KI_ADD;
        case SDLK_KP_MINUS: return Rml::Input::KI_SUBTRACT;
        case SDLK_KP_PERIOD: return Rml::Input::KI_DECIMAL;
        case SDLK_KP_DIVIDE: return Rml::Input::KI_DIVIDE;
        case SDLK_KP_EQUALS: return Rml::Input::KI_OEM_NEC_EQUAL;
        case SDLK_BACKSPACE: return Rml::Input::KI_BACK;
        case SDLK_TAB: return Rml::Input::KI_TAB;
        case SDLK_CLEAR: return Rml::Input::KI_CLEAR;
        case SDLK_RETURN: return Rml::Input::KI_RETURN;
        case SDLK_PAUSE: return Rml::Input::KI_PAUSE;
        case SDLK_CAPSLOCK: return Rml::Input::KI_CAPITAL;
        case SDLK_PAGEUP: return Rml::Input::KI_PRIOR;
        case SDLK_PAGEDOWN: return Rml::Input::KI_NEXT;
        case SDLK_END: return Rml::Input::KI_END;
        case SDLK_HOME: return Rml::Input::KI_HOME;
        case SDLK_LEFT: return Rml::Input::KI_LEFT;
        case SDLK_UP: return Rml::Input::KI_UP;
        case SDLK_RIGHT: return Rml::Input::KI_RIGHT;
        case SDLK_DOWN: return Rml::Input::KI_DOWN;
        case SDLK_INSERT: return Rml::Input::KI_INSERT;
        case SDLK_DELETE: return Rml::Input::KI_DELETE;
        case SDLK_HELP: return Rml::Input::KI_HELP;
        case SDLK_NUMLOCKCLEAR: return Rml::Input::KI_NUMLOCK;
        case SDLK_SCROLLLOCK: return Rml::Input::KI_SCROLL;
        case SDLK_LSHIFT: return Rml::Input::KI_LSHIFT;
        case SDLK_RSHIFT: return Rml::Input::KI_RSHIFT;
        case SDLK_LCTRL: return Rml::Input::KI_LCONTROL;
        case SDLK_RCTRL: return Rml::Input::KI_RCONTROL;
        case SDLK_LALT: return Rml::Input::KI_LMENU;
        case SDLK_RALT: return Rml::Input::KI_RMENU;
        case SDLK_LGUI: return Rml::Input::KI_LMETA;
        case SDLK_RGUI: return Rml::Input::KI_RMETA;
        default: return Rml::Input::KI_UNKNOWN;
    }
}

class WeilanRmlUiSystem : public Rml::SystemInterface
{
public:
    double GetElapsedTime() override
    {
        using Clock = std::chrono::steady_clock;
        return std::chrono::duration<double>(Clock::now() - startTime).count();
    }

    void JoinPath(Rml::String& translatedPath, const Rml::String& documentPath, const Rml::String& path) override
    {
        std::filesystem::path resourcePath(path);
        if (resourcePath.is_absolute())
        {
            translatedPath = resourcePath.lexically_normal().generic_string();
            return;
        }

        std::filesystem::path joinedPath = documentPath.empty() ? resourcePath : GetDocumentBasePath(documentPath) / resourcePath;
        translatedPath = ResolveProjectPath(joinedPath).generic_string();
    }

    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override
    {
        switch (type)
        {
            case Rml::Log::LT_ERROR:
            case Rml::Log::LT_ASSERT:
                spdlog::error("RmlUi: {}", message);
                break;
            case Rml::Log::LT_WARNING:
                spdlog::warn("RmlUi: {}", message);
                break;
            case Rml::Log::LT_INFO:
                spdlog::info("RmlUi: {}", message);
                break;
            default:
                spdlog::debug("RmlUi: {}", message);
                break;
        }
        return true;
    }

    void SetClipboardText(const Rml::String& text) override
    {
        SDL_SetClipboardText(text.c_str());
    }

    void GetClipboardText(Rml::String& text) override
    {
        char* clipboardText = SDL_GetClipboardText();
        if (clipboardText)
        {
            text = clipboardText;
            SDL_free(clipboardText);
        }
        else
        {
            text.clear();
        }
    }

private:
    std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
};
} // namespace

UI::UI()
{}

void UI::InitLuaBinding()
{
    Rml::Lua::Initialise(LuaBackend::L);
    LuaBackend::RestoreEnginePrint();
    const int topnums = lua_gettop(LuaBackend::L);
    lua_pop(LuaBackend::L, topnums);
}

void UI::Init()
{
    if (rmlInitialized)
    {
        return;
    }

    activeUI = this;
    whiteTexture = &EngineInternalResources::GetWhiteTexture();
    cmd = GetGfxDriver()->CreateCommandBuffer();

    if (auto driver = GetGfxDriver())
    {
        const auto surfaceSize = driver->GetSurfaceSize();
        canvasSize = {static_cast<int>(surfaceSize.width), static_cast<int>(surfaceSize.height)};
    }

    rmlFileInterface = std::make_unique<WeilanRmlUiFileInterface>();
    rmlSystem = std::make_unique<WeilanRmlUiSystem>();
    rmlRenderer = std::make_unique<RmlUiRenderer>();
    Rml::SetFileInterface(rmlFileInterface.get());
    Rml::SetSystemInterface(rmlSystem.get());
    Rml::SetRenderInterface(rmlRenderer.get());

    Rml::Initialise();

    InitLuaBinding();

    const std::filesystem::path defaultFontPath = std::filesystem::path(ENGINE_SOURCE_PATH) / "Resources" / "MononokiNerdFont-Regular.ttf";
    if (!Rml::LoadFontFace(defaultFontPath.string()))
    {
        spdlog::warn("Failed to load RmlUi default font: {}", defaultFontPath.string());
    }
    rmlInitialized = true;
    rmlContext = Rml::CreateContext("GameUI", Rml::Vector2i(canvasSize.x, canvasSize.y));
    rmlDebuggerInitialized = Rml::Debugger::Initialise(rmlContext);
    if (rmlDebuggerInitialized)
    {
        Rml::Debugger::SetVisible(rmlDebuggerVisible);
    }
}

void UI::Destroy()
{
    if (activeUI == this)
    {
        activeUI = nullptr;
    }

    if (rmlInitialized)
    {
        if (rmlDebuggerInitialized)
        {
            Rml::Debugger::Shutdown();
            rmlDebuggerInitialized = false;
        }
        Rml::Shutdown();
        rmlContext = nullptr;
        rmlInitialized = false;
    }

    DelayDestroy::Singleton()->Destory(std::move(rmlRenderer));
}

UI::~UI() {}

void UI::SetUICanvasCoordinate(int2 origin, int2 size)
{
    canvasOrigin = origin;
    canvasSize = {std::max(1, size.x), std::max(1, size.y)};
    if (rmlContext)
    {
        rmlContext->SetDimensions(Rml::Vector2i(canvasSize.x, canvasSize.y));
    }
}

void UI::Update()
{
    if (rmlContext)
    {
        rmlContext->Update();
    }
}

Rml::ElementDocument* UI::LoadDocument(std::string_view path)
{
    if (!rmlContext)
    {
        return nullptr;
    }

    std::string documentPath(path);
    Rml::ElementDocument* document = rmlContext->LoadDocument(documentPath);
    if (document)
    {
        document->Show();
    }
    return document;
}

bool UI::LoadFontFace(std::string_view path, bool fallbackFace)
{
    std::string fontPath(path);
    return Rml::LoadFontFace(fontPath, fallbackFace);
}

Rml::Context* UI::GetRmlContext()
{
    return rmlContext;
}

void UI::SetRmlDebuggerVisible(bool visible)
{
    rmlDebuggerVisible = visible;
    if (rmlDebuggerInitialized)
    {
        Rml::Debugger::SetVisible(visible);
    }
}

void UI::ToggleRmlDebugger()
{
    SetRmlDebuggerVisible(!IsRmlDebuggerVisible());
}

bool UI::IsRmlDebuggerVisible() const
{
    if (rmlDebuggerInitialized)
    {
        return Rml::Debugger::IsVisible();
    }
    return rmlDebuggerVisible;
}

bool UI::ProcessSDLEvent(const SDL_Event& event)
{
    bool consumeInput = false;

    if (!activeUI || !activeUI->rmlContext)
    {
        return false;
    }

    auto toGameViewPosition = [](int x, int y)
    {
        int2 gameViewOrigin = SystemInfo::Singleton().GetGameViewOrigin();
        float2 screenResolution = SystemInfo::Singleton().GetScreenResolution();
        float2 screenSize = SystemInfo::Singleton().GetScreenSize();
        float2 uiCoordRemapped = float2(x - gameViewOrigin.x, y - gameViewOrigin.y) / screenSize * screenResolution;
        return Rml::Vector2i(int(uiCoordRemapped.x), int(uiCoordRemapped.y));
    };

    switch (event.type)
    {
        case SDL_MOUSEMOTION:
            {
                Rml::Vector2i pos = toGameViewPosition(event.motion.x, event.motion.y);
                activeUI->rmlContext->ProcessMouseMove(pos.x, pos.y, 0);
                break;
            }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            {
                int button = -1;
                if (event.button.button == SDL_BUTTON_LEFT)
                    button = 0;
                else if (event.button.button == SDL_BUTTON_RIGHT)
                    button = 1;
                else if (event.button.button == SDL_BUTTON_MIDDLE)
                    button = 2;

                if (button >= 0)
                {
                    Rml::Vector2i pos = toGameViewPosition(event.button.x, event.button.y);
                    activeUI->rmlContext->ProcessMouseMove(pos.x, pos.y, 0);
                    if (event.type == SDL_MOUSEBUTTONDOWN)
                        consumeInput = activeUI->rmlContext->ProcessMouseButtonDown(button, 0);
                    else
                        consumeInput = activeUI->rmlContext->ProcessMouseButtonUp(button, 0);
                }
                break;
            }
        case SDL_MOUSEWHEEL:
            activeUI->rmlContext->ProcessMouseWheel(static_cast<float>(event.wheel.y), 0);
            break;
        case SDL_TEXTINPUT:
            activeUI->rmlContext->ProcessTextInput(event.text.text);
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            {
                Rml::Input::KeyIdentifier key = ConvertSDLKey(event.key.keysym.sym);
                if (key != Rml::Input::KI_UNKNOWN)
                {
                    int modifiers = GetRmlKeyModifierState();
                    if (event.type == SDL_KEYDOWN)
                        activeUI->rmlContext->ProcessKeyDown(key, modifiers);
                    else
                        activeUI->rmlContext->ProcessKeyUp(key, modifiers);
                }
                break;
            }
        default:
            break;
    }

    return consumeInput;
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
    if (colorImage == nullptr || rmlContext == nullptr)
        return;

    Gfx::RenderAttachment colorAttachment[1] =
        {
            {
                .image = *colorImage,
                .loadOp = Gfx::AttachmentLoadOperation::Load,
                .storeOp = Gfx::AttachmentStoreOperation::Store,
            }
        };
    Gfx::ClearValue clearValues[1] = {{0.0f, 0.0f, 0.0f, 0.0f}};

    cmd->BeginRenderPass(colorAttachment, clearValues);

    if (rmlContext && rmlRenderer)
    {
        rmlRenderer->BeginFrame(*cmd, canvasOrigin, canvasSize);
        rmlContext->Render();
        rmlRenderer->EndFrame();
    }

    cmd->EndRenderPass();
    GetGfxDriver()->ExecuteCommandBuffer(*cmd);
    cmd->Reset(true);

    uiElements.clear();
}

UI& UI::Instance()
{
    static UI ui;
    return ui;
}

void UI::ClearRmlUiCache()
{
    Rml::Factory::ClearStyleSheetCache();
    Rml::Factory::ClearTemplateCache();
}

void UI::ReloadRmlUiResources()
{
    UI& ui = Instance();
    const bool debuggerWasInitialized = ui.rmlDebuggerInitialized;
    const bool debuggerWasVisible = debuggerWasInitialized && Rml::Debugger::IsVisible();

    if (debuggerWasInitialized)
    {
        Rml::Debugger::Shutdown();
        ui.rmlDebuggerInitialized = false;
    }

    Rml::Factory::ClearStyleSheetCache();
    Rml::Factory::ClearTemplateCache();
    Rml::ReleaseTextures();

    if (debuggerWasInitialized && ui.rmlContext)
    {
        ui.rmlDebuggerInitialized = Rml::Debugger::Initialise(ui.rmlContext);
        if (ui.rmlDebuggerInitialized)
        {
            Rml::Debugger::SetVisible(debuggerWasVisible);
        }
    }
}
