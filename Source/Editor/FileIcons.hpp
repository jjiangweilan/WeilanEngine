#pragma once
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Texture.hpp"
#include <filesystem>
#include <typeindex>
#include <unordered_map>

class FileIcons
{
public:
    static FileIcons& Instance();
    // enter utf code picked from here: https://www.nerdfonts.com/cheat-sheet
    static std::string Utf16ToUtf8(char16_t utf16_codepoint);
    static std::string GetIcon(const std::filesystem::path& path);
    Gfx::Image* GetIconImage(const std::filesystem::path& path);
    Gfx::Image* GetDirectoryIconImage() { return fileIcon->GetGfxImage(); }

private:
    static std::unordered_map<std::string, std::string> toIcon;
    static std::unordered_map<std::type_index, char16_t> toIconType;

    struct
    {
        std::unordered_map<std::filesystem::path, ObjPtr<Texture>> caches;
    } previewImages;

    /**
     * @brief load using asset loader's type index as key, if it's not feasiable then use fallbackExtension
     *
     * @param assetLoaderTypeIndex the asset loader's type idex
     * @param fallbackExtension fallback extension
     */
    Gfx::Image* GetFileIcon(
        const std::type_index& assetLoaderTypeIndex, const std::filesystem::path& fallbackExtension
    );
    std::unordered_map<std::string, Gfx::Image*> toIconImage;

    Gfx::Image* GetDefaultFileIcon() { return documentIcon->GetGfxImage(); }

    LazyLoadedAsset<Texture> fileIcon = "_engine_internal/Editor/Icons/folder.png";
    LazyLoadedAsset<Texture> textureIcon = "_engine_internal/Editor/Icons/texture.png";
    LazyLoadedAsset<Texture> luaIcon = "_engine_internal/Editor/Icons/lua.png";
    LazyLoadedAsset<Texture> modelIcon = "_engine_internal/Editor/Icons/3d-model.png";
    LazyLoadedAsset<Texture> documentIcon = "_engine_internal/Editor/Icons/document.png";
    LazyLoadedAsset<Texture> sceneIcon = "_engine_internal/Editor/Icons/scene.png";
    LazyLoadedAsset<Texture> prefabIcon = "_engine_internal/Editor/Icons/prefab.png";
    LazyLoadedAsset<Texture> renderPipelineIcon = "_engine_internal/Editor/Icons/render-pipeline.png";

    Gfx::Image* LoadPreviewImage(const AssetPath& path);
};
