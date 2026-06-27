#pragma once

#include "Editor/Window.hpp"
#include "Engine/Core/Ptr.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetPath.hpp"
#include <string>

class CursorAtlas;
class Texture;

namespace Editor
{
class CursorAtlasEditorWindow : public Window
{
public:
    DECLARE_EDITOR_WINDOW(CursorAtlasEditorWindow)

    bool Tick() override;

private:
    ObjPtr<Texture> sourceTexture;
    ObjPtr<CursorAtlas> targetAtlas;
    std::string outputPath = "New CursorAtlas";
    int frameWidth = 32;
    int frameHeight = 32;
    int columns = 1;
    int rows = 1;
    int selectedIndex = 0;
    int hotspotX = 0;
    int hotspotY = 0;
    std::string status;

    bool BuildAtlas(CursorAtlas& atlas);
};
} // namespace Editor
