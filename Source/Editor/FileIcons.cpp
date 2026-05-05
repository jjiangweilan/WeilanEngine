#include "FileIcons.hpp"
#include "Editor/ModelPreviewRenderer.hpp"
#include "Engine/Runtime/System/AssetDatabase/Loaders/AssetLoader.hpp"
#include "Engine/Runtime/System/AssetDatabase/Loaders/ModelLoader.hpp"
#include "Engine/Runtime/System/AssetDatabase/Loaders/TextureLoader.hpp"
#include <codecvt>
#include <locale>

std::string FileIcons::Utf16ToUtf8(char16_t utf16_codepoint)
{
    // Convert UTF-16 to UTF-32 (widening)
    std::u16string utf16_str(1, utf16_codepoint);
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert_utf16_to_utf8;
    return convert_utf16_to_utf8.to_bytes(utf16_str);
}

void FileIcons::Initialize(WeilanEngine* engine)
{
    this->engine = engine;
}

void FileIcons::Shutdown()
{
    modelPreviewRenderer.reset();
    engine = nullptr;
}

Gfx::Image* FileIcons::LoadPreviewImage(const AssetPath& path)
{
    auto iter = previewImages.caches.find(path);
    if (iter != previewImages.caches.end() && iter->second != nullptr)
    {
        return iter->second->GetGfxImage();
    }

    std::unique_ptr<AssetLoader> loader = AssetLoaderRegistry::CreateAssetLoaderByExtension(path.GetExtension());
    if (loader == nullptr || typeid(*loader) != typeid(TextureLoader))
    {
        return nullptr;
    }

    auto texture = dynamic_cast<Texture*>(AssetDatabase::Singleton()->LoadAsset(path));
    if (texture)
    {
        previewImages.caches[path] = texture;
        return texture->GetGfxImage();
    }

    return nullptr;
}

Gfx::Image* FileIcons::GetIconImage(const AssetPath& path)
{
    Gfx::Image* previewImage = LoadPreviewImage(path);
    if (previewImage)
        return previewImage;

    std::string ext = path.GetExtension();
    auto iter = toIconImage.find(ext);
    if (iter != toIconImage.end())
    {
        return iter->second;
    }
    else
    {
        std::unique_ptr<AssetLoader> loader = AssetLoaderRegistry::CreateAssetLoaderByExtension(ext);

        // Null protection
        if (loader == nullptr)
        {
            return GetDefaultFileIcon();
        }

        // Load file icon from preset
        auto configuredFileIcon = GetFileIcon(typeid(*loader), ext);

        if ((typeid(*loader) == typeid(ModelLoader) || ext == ".prefab") && engine != nullptr)
        {
            if (modelPreviewRenderer == nullptr)
            {
                modelPreviewRenderer = std::make_unique<ModelPreviewRenderer>(engine);
            }

            if (Gfx::Image* modelPreview = modelPreviewRenderer->GetOrQueuePreview(path))
            {
                return modelPreview;
            }
        }

        if (configuredFileIcon)
        {
            return configuredFileIcon;
        }
    }

    // Nothing works, just return default file icon
    return GetDefaultFileIcon();
}

Gfx::Image* FileIcons::GetModelPreviewImage(const AssetPath& path)
{
    if (engine == nullptr || path.empty())
    {
        return nullptr;
    }

    std::unique_ptr<AssetLoader> loader = AssetLoaderRegistry::CreateAssetLoaderByExtension(path.GetExtension());
    if (loader == nullptr || typeid(*loader) != typeid(ModelLoader))
    {
        return nullptr;
    }

    if (modelPreviewRenderer == nullptr)
    {
        modelPreviewRenderer = std::make_unique<ModelPreviewRenderer>(engine);
    }

    return modelPreviewRenderer->GetOrQueuePreview(path);
}

void FileIcons::RenderQueuedPreviews(Gfx::CommandBuffer& cmd)
{
    if (modelPreviewRenderer != nullptr)
    {
        modelPreviewRenderer->RenderQueuedPreviews(cmd, 1);
    }
}

std::string FileIcons::GetIcon(const AssetPath& path)
{
    std::string ext = path.GetExtension();
    auto iter = toIcon.find(ext);
    if (iter != toIcon.end())
    {
        return iter->second;
    }
    else
    {
        std::unique_ptr<AssetLoader> loader = AssetLoaderRegistry::CreateAssetLoaderByExtension(ext);

        if (loader == nullptr)
        {
            toIcon[ext] = "";
            return "";
        }

        auto iter = toIconType.find(typeid(*loader));
        if (iter != toIconType.end())
        {
            auto val = Utf16ToUtf8(iter->second);
            toIcon[ext] = val;
            return val;
        }
    }

    return "";
}

std::unordered_map<std::string, std::string> FileIcons::toIcon = std::unordered_map<std::string, std::string>();

static std::unordered_map<std::type_index, char16_t> InitToIconType()
{
    return {{typeid(ModelLoader), 0xe735}, {typeid(TextureLoader), 0xf03e}};
}
std::unordered_map<std::type_index, char16_t> FileIcons::toIconType = InitToIconType();

FileIcons& FileIcons::Instance()
{
    static FileIcons instance;
    return instance;
}

Gfx::Image* FileIcons::GetFileIcon(const std::type_index& typeIndex, const std::string& fallbackExtension)
{
    if (typeIndex == typeid(ModelLoader))
        return modelIcon->GetGfxImage();
    else if (typeIndex == typeid(TextureLoader))
        return textureIcon->GetGfxImage();

    if (fallbackExtension == ".lua")
        return luaIcon->GetGfxImage();
    else if (fallbackExtension == ".scene")
        return sceneIcon->GetGfxImage();
    else if (fallbackExtension == ".prefab")
        return prefabIcon->GetGfxImage();
    else if (fallbackExtension == ".renderPipeline")
        return renderPipelineIcon->GetGfxImage();

    return GetDefaultFileIcon();
}
