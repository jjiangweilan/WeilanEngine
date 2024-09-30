#include "../EditorState.hpp"
#include "AssetDatabase/AssetDatabase.hpp"
#include "Core/Texture.hpp"
#include "Inspector.hpp"
namespace Editor
{
class TextureInspector : public Inspector<Texture>
{
public:
    void OnEnable(Object& obj) override
    {
        Inspector<Texture>::OnEnable(obj);

        layer = 0;
        mip = 0;
        if (!target->GetDescription().img.isCubemap)
        {
            imageViewInUse = &target->GetGfxImage()->GetDefaultImageView();
        }
        else
        {
            imageView = GetGfxDriver()->CreateImageView(
                {.image = *target->GetGfxImage(),
                 .imageViewType = Gfx::ImageViewType::Image_2D,
                 .subresourceRange = Gfx::ImageSubresourceRange{Gfx::ImageAspect::Color, 0, 1, layer, 1}}
            );
            imageViewInUse = imageView.get();
        }
    }
    void DrawInspector(GameEditor& editor) override
    {
        // object information
        auto& name = target->GetName();
        char cname[1024];
        strcpy(cname, name.data());
        if (ImGui::InputText("Name", cname, 1024))
        {
            target->SetName(cname);
        }

        auto width = target->GetDescription().img.width;
        auto height = target->GetDescription().img.height;
        auto contentMax = ImGui::GetWindowContentRegionMax();
        auto contentMin = ImGui::GetWindowContentRegionMin();
        auto contentWidth = contentMax.x - contentMin.y;
        auto contentHeight = contentMax.y - contentMin.y;
        auto size = ResizeKeepRatio(width, height, contentWidth, contentHeight);

        if (reimport)
        {
            AssetDatabase::Singleton()->LoadAssetByID(target->GetUUID(), true);
        }

        ImGui::NewLine();
        ImGui::Text("Preview");
        ImGui::Separator();
        UpdateImageView();
        glm::vec2 displaySize = size;
        displaySize.x = glm::min(ImGui::GetContentRegionMax().x, displaySize.x);
        ImGui::Image(imageViewInUse, {displaySize.x, displaySize.y * width / height});
        ImGui::Separator();
        float mb = target->GetDescription().img.GetByteSize() / 1024.0f / 1024.0f;
        ImGui::Text("size: %d x %d", width, height);
        ImGui::Text("memory size (without mip): %f Mb", mb);
        ImGui::Text("format %s", Gfx::MapImageFormatToString(target->GetDescription().img.format));

        // show and update meta
        ImGui::NewLine();
        auto meta = AssetDatabase::Singleton()->GetAssetMeta(*target);
        auto options = meta.value("importOption", nlohmann::json::object());
        bool generateMipmap = options.value("generateMipmap", false);
        bool convertToIrradianceCubemap = options.value("convertToIrradianceCubemap", false);
        bool converToCubemap = options.value("convertToCubemap", false);
        bool convertToReflectanceCubemap = options.value("convertToReflectanceCubemap", false);

        ImGui::Text("Import Options");
        ImGui::Separator();
        bool metaChanged = false;
        metaChanged |= ImGui::Checkbox("generateMipmap", &generateMipmap);
        metaChanged |= ImGui::Checkbox("convertToCubemap", &converToCubemap);
        metaChanged |= ImGui::Checkbox("convertToIrradianceCubemap", &convertToIrradianceCubemap);
        metaChanged |= ImGui::Checkbox("convertToReflectanceCubemap", &convertToReflectanceCubemap);
        if (metaChanged)
        {
            meta["importOption"]["generateMipmap"] = generateMipmap;
            meta["importOption"]["convertToCubemap"] = converToCubemap;
            meta["importOption"]["convertToIrradianceCubemap"] = convertToIrradianceCubemap;
            meta["importOption"]["convertToReflectanceCubemap"] = convertToReflectanceCubemap;
        }
        if (metaChanged)
            AssetDatabase::Singleton()->SetAssetMeta(*target, meta);

        // import button
        if (ImGui::Button("Reimport"))
        {
            reimport = true;
        }
    }

private:
    static const char _register;
    Gfx::ImageView* imageViewInUse;
    std::unique_ptr<Gfx::ImageView> imageView;
    uint32_t layer = 0;
    uint32_t mip = 0;
    bool reimport = false;

    glm::vec2 ResizeKeepRatio(float width, float height, float contentWidth, float contentHeight)
    {
        float imageWidth = width;
        float imageHeight = height;

        // shrink width
        if (imageWidth > contentWidth)
        {
            float ratio = contentWidth / (float)imageWidth;
            imageWidth = contentWidth;
            imageHeight *= ratio;
        }

        if (imageHeight > contentHeight)
        {
            float ratio = contentHeight / (float)imageHeight;
            imageHeight = contentHeight;
            imageWidth *= ratio;
        }

        return {imageWidth, imageHeight};
    }

    void UpdateImageView()
    {
        int layer_i = layer;
        int mip_i = mip;
        bool changeImageView = ImGui::InputInt("mip", &mip_i);
        changeImageView = ImGui::InputInt("layer", &layer_i) || changeImageView;
        if (reimport || changeImageView)
        {
            if (reimport || ((layer_i >= 0 && layer_i != layer && layer_i < target->GetDescription().img.GetLayer()) ||
                             (mip_i >= 0 && mip_i != mip && mip_i < target->GetDescription().img.mipLevels)))
            {
                reimport = false;
                layer = layer_i;
                mip = mip_i;
                imageView = GetGfxDriver()->CreateImageView(
                    {.image = *target->GetGfxImage(),
                     .imageViewType = Gfx::ImageViewType::Image_2D,
                     .subresourceRange = Gfx::ImageSubresourceRange{Gfx::ImageAspect::Color, mip, 1, layer, 1}}
                );
                imageViewInUse = imageView.get();
            }
        }
    }
};

const char TextureInspector::_register = InspectorRegistry::Register<TextureInspector, Texture>();

} // namespace Editor
