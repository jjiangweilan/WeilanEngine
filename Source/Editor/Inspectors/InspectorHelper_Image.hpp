#pragma once
#include "Core/DelayDestroy.hpp"
#include "GfxDriver/GfxDriver.hpp"
#include "ThirdParty/imgui/imgui.h"

namespace Editor
{
class ImageInspector
{
public:
    void Initialize(Gfx::Image* gfxImage)
    {
        bool isCubemap = gfxImage->GetDescription().isCubemap;
        layer = 0;
        mip = 0;
        this->gfxImage = gfxImage;
        if (!isCubemap)
        {
            imageViewInUse = &gfxImage->GetDefaultImageView();
        }
        else
        {
            if (imageView != nullptr)
            {
                DelayDestroy::Singleton()->Destory(std::move(imageView));
            }
            imageView = GetGfxDriver()->CreateImageView(
                {.image = *gfxImage,
                 .imageViewType = Gfx::ImageViewType::Image_2D,
                 .subresourceRange = Gfx::ImageSubresourceRange{Gfx::ImageAspect::Color, 0, 1, layer, 1}}
            );
            imageViewInUse = imageView.get();
        }
    }

    void ShowImage(float imageScale)
    {
        auto width = gfxImage->GetDescription().width;
        auto height = gfxImage->GetDescription().height;
        auto contentWidth = height > ImGui::GetWindowWidth() ? ImGui::GetWindowWidth() : width;
        auto size = ResizeKeepRatio(width, height, contentWidth, height) * imageScale;
        UpdateImageView(gfxImage->GetDescription().GetLayer(), gfxImage->GetDescription().mipLevels);
        ImGui::Image(GetImageViewInUse(), {size.x, size.y});
    }

    void SetReimport(bool value) { reimport = value; }

private:
    Gfx::Image* gfxImage;
    Gfx::ImageView* imageViewInUse;
    std::unique_ptr<Gfx::ImageView> imageView;
    uint32_t layer = 0;
    uint32_t mip = 0;
    bool reimport = false;

    Gfx::ImageView* GetImageViewInUse() const { return imageViewInUse; }

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
    void UpdateImageView(uint32_t imgLayer, uint32_t imgMipLevels)
    {
        int layer_i = layer;
        int mip_i = mip;
        bool changeImageView = ImGui::InputInt("mip", &mip_i);
        changeImageView = ImGui::InputInt("layer", &layer_i) || changeImageView;
        if (reimport || changeImageView)
        {
            if (reimport || ((layer_i >= 0 && layer_i != layer && layer_i < imgLayer) ||
                             (mip_i >= 0 && mip_i != mip && mip_i < imgMipLevels)))
            {
                reimport = false;
                layer = layer_i;
                mip = mip_i;
                if (imageView != nullptr)
                {
                    DelayDestroy::Singleton()->Destory(std::move(imageView));
                }
                imageView = GetGfxDriver()->CreateImageView(
                    {.image = *gfxImage,
                     .imageViewType = Gfx::ImageViewType::Image_2D,
                     .subresourceRange = Gfx::ImageSubresourceRange{Gfx::ImageAspect::Color, mip, 1, layer, 1}}
                );
                imageView->SetName("imageDisplay-ImageInspector");
                imageViewInUse = imageView.get();
            }
        }
    }
};

} // namespace Editor
