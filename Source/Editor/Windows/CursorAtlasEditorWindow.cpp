#include "CursorAtlasEditorWindow.hpp"

#include "Editor/EditorGUI.hpp"
#include "Engine/Runtime/Object/Texture/CursorAtlas.hpp"
#include "Engine/Runtime/Object/Texture/Texture.hpp"
#include "Engine/Runtime/System/AssetDatabase/AssetDatabase.hpp"
#include "Engine/ThirdParty/imgui/imgui.h"
#include "Engine/ThirdParty/stb/stb_image.h"
#include <algorithm>
#include <memory>

namespace Editor
{
DEFINE_EDITOR_WINDOW(CursorAtlasEditorWindow, "Tools/Cursor Atlas Maker")

bool CursorAtlasEditorWindow::BuildAtlas(CursorAtlas& atlas)
{
    Texture* texture = sourceTexture.Get();
    if (texture == nullptr)
    {
        status = "Select a source texture.";
        return false;
    }

    if (frameWidth <= 0 || frameHeight <= 0 || columns <= 0 || rows <= 0)
    {
        status = "Frame size and grid must be positive.";
        return false;
    }

    std::vector<uint8_t> rawData = AssetDatabase::Singleton()->ReadRawAssetData(texture->GetUUID());
    if (rawData.empty())
    {
        status = "Failed to read source texture data.";
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc* decoded = stbi_load_from_memory(rawData.data(), static_cast<int>(rawData.size()), &width, &height, &channels, 4);
    if (decoded == nullptr || width <= 0 || height <= 0)
    {
        if (decoded != nullptr)
            stbi_image_free(decoded);
        status = "Failed to decode source texture as RGBA8.";
        return false;
    }

    if (columns * frameWidth > width || rows * frameHeight > height)
    {
        stbi_image_free(decoded);
        status = "Frame grid exceeds source texture bounds.";
        return false;
    }

    atlas.atlasWidth = static_cast<uint32_t>(width);
    atlas.atlasHeight = static_cast<uint32_t>(height);
    atlas.frameWidth = static_cast<uint32_t>(frameWidth);
    atlas.frameHeight = static_cast<uint32_t>(frameHeight);
    atlas.columns = static_cast<uint32_t>(columns);
    atlas.rows = static_cast<uint32_t>(rows);
    atlas.rgbaPixels.resize(static_cast<size_t>(width) * height);

    for (int i = 0; i < width * height; ++i)
    {
        const size_t src = static_cast<size_t>(i) * 4;
        atlas.rgbaPixels[i] = static_cast<uint32_t>(decoded[src + 0]) |
                              (static_cast<uint32_t>(decoded[src + 1]) << 8) |
                              (static_cast<uint32_t>(decoded[src + 2]) << 16) |
                              (static_cast<uint32_t>(decoded[src + 3]) << 24);
    }

    stbi_image_free(decoded);

    const int cursorCount = atlas.GetCursorCount();
    atlas.hotspots.resize(cursorCount);
    const int clampedIndex = std::clamp(selectedIndex, 0, std::max(0, cursorCount - 1));
    atlas.hotspots[clampedIndex].x = static_cast<uint32_t>(std::clamp(hotspotX, 0, frameWidth - 1));
    atlas.hotspots[clampedIndex].y = static_cast<uint32_t>(std::clamp(hotspotY, 0, frameHeight - 1));
    atlas.SetDirty();
    status = "Cursor atlas built.";
    return true;
}

bool CursorAtlasEditorWindow::Tick()
{
    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(440, 420), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Cursor Atlas Maker", &open))
    {
        Texture* source = sourceTexture.Get();
        if (EditorGUI::ObjectField("Source Texture", source))
        {
            sourceTexture = source;
            if (source != nullptr)
            {
                const auto& desc = source->GetDescription().img;
                if (desc.width > 0 && desc.height > 0)
                {
                    columns = std::max(1, static_cast<int>(desc.width) / std::max(1, frameWidth));
                    rows = std::max(1, static_cast<int>(desc.height) / std::max(1, frameHeight));
                }
            }
        }

        CursorAtlas* target = targetAtlas.Get();
        if (EditorGUI::ObjectField("Existing Atlas", target))
        {
            targetAtlas = target;
            if (target != nullptr)
            {
                frameWidth = static_cast<int>(target->frameWidth);
                frameHeight = static_cast<int>(target->frameHeight);
                columns = static_cast<int>(target->columns);
                rows = static_cast<int>(target->rows);
            }
        }

        EditorGUI::InputTextLabeled("Output Path", outputPath);
        ImGui::DragInt("Frame Width", &frameWidth, 1.0f, 1, 512);
        ImGui::DragInt("Frame Height", &frameHeight, 1.0f, 1, 512);
        ImGui::DragInt("Columns", &columns, 1.0f, 1, 256);
        ImGui::DragInt("Rows", &rows, 1.0f, 1, 256);

        int cursorCount = std::max(1, columns * rows);
        selectedIndex = std::clamp(selectedIndex, 0, cursorCount - 1);
        ImGui::SliderInt("Texture Index", &selectedIndex, 0, cursorCount - 1);
        ImGui::DragInt("Hotspot X", &hotspotX, 1.0f, 0, std::max(0, frameWidth - 1));
        ImGui::DragInt("Hotspot Y", &hotspotY, 1.0f, 0, std::max(0, frameHeight - 1));

        if (source != nullptr && source->GetGfxImage() != nullptr)
        {
            ImGui::Text("Source Preview");
            ImGui::Image(&source->GetGfxImage()->GetDefaultImageView(), ImVec2(128, 128));
        }

        if (ImGui::Button("Create Cursor Atlas"))
        {
            auto atlas = std::make_unique<CursorAtlas>();
            if (BuildAtlas(*atlas))
            {
                Asset* saved = AssetDatabase::Singleton()->SaveAsset(std::move(atlas), outputPath);
                targetAtlas = dynamic_cast<CursorAtlas*>(saved);
                status = saved != nullptr ? "Created cursor atlas." : "Failed to save cursor atlas.";
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Update Existing"))
        {
            CursorAtlas* atlas = targetAtlas.Get();
            if (atlas != nullptr && BuildAtlas(*atlas))
            {
                AssetDatabase::Singleton()->SaveAsset(*atlas);
                status = "Updated cursor atlas.";
            }
            else if (atlas == nullptr)
            {
                status = "Assign an existing atlas to update.";
            }
        }

        if (!status.empty())
        {
            ImGui::Separator();
            ImGui::TextWrapped("%s", status.c_str());
        }
    }
    ImGui::End();
    return open;
}
} // namespace Editor
