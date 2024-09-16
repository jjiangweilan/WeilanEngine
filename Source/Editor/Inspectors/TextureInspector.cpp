#include "../EditorState.hpp"
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

        int layer_i = layer;
        if (ImGui::InputInt("layer", &layer_i))
        {
            if (layer_i >= 0 && layer_i != layer && layer_i < target->GetDescription().img.GetLayer())
            {
                layer = layer_i;
                imageView = GetGfxDriver()->CreateImageView(
                    {.image = *target->GetGfxImage(),
                     .imageViewType = Gfx::ImageViewType::Image_2D,
                     .subresourceRange = Gfx::ImageSubresourceRange{Gfx::ImageAspect::Color, 0, 1, layer, 1}}
                );
                imageViewInUse = imageView.get();
            }
        }

        ImGui::Image(imageViewInUse, {size.x, size.y});
        ImGui::Separator();
        float mb =target->GetDescription().img.GetByteSize() / 1024.0f / 1024.0f;
        ImGui::Text("size: %d x %d", width, height);
        ImGui::Text("disk size: %f Mb", mb);

        if(ImGui::Button("Bake to cubemap"))
        {
            target->SaveAsCubemap("gogo");
        }
    }

private:
    static const char _register;
    Gfx::ImageView* imageViewInUse;
    std::unique_ptr<Gfx::ImageView> imageView;
    uint32_t layer = 0;

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
};

const char TextureInspector::_register = InspectorRegistry::Register<TextureInspector, Texture>();

} // namespace Editor
