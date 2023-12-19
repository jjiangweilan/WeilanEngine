#pragma once
#include "LinearImage.hpp"
#include "ThirdParty/stb/stb_image.h"
#include "ThirdParty/stb/stb_image_write.h"
#include "Utils/Structs.hpp"
#include <filesystem>
#include <format>
namespace Libs::Image
{
class VirtualTextureConverter
{
public:
    // nomeclature
    // x, y: page space
    // w, h: pixel space
    void Convert(
        const std::filesystem::path& srcImage,
        const std::filesystem::path& dstFolder,
        int mip,
        int desiredChannels,
        uint32_t pageExtent
    )
    {
        int w, h, channels;
        stbi_uc* data = stbi_load(srcImage.string().c_str(), &w, &h, &channels, desiredChannels);

        if (w % pageExtent != 0 || h % pageExtent != 0)
            return;

        int pageXCount = std::ceil(w / (float)pageExtent);
        int pageYCount = std::ceil(h / (float)pageExtent);

        Extent2D srcWh{(uint32_t)w, (uint32_t)h};
        for (int i = 0; i < pageXCount; ++i)
        {
            for (int j = 0; j < pageYCount; ++j)
            {
                Rect2D pageRect;
                pageRect.offset = {i * (int32_t)pageExtent, j * (int32_t)pageExtent};
                pageRect.extent = {pageExtent, pageExtent};
                LinearImage page = ExtractPage(data, srcWh, pageRect, channels);

                std::string name = fmt::format("{}_{}_{}.jpg", mip, i, j);
                std::filesystem::path dstPath = dstFolder / name;
                stbi_write_jpg(dstPath.string().c_str(), pageExtent, pageExtent, channels, page.GetData(), 100);
            }
        }

        delete data;
    }

private:
    const int ElementSize = 1; // expect 8 bit channel

    LinearImage ExtractPage(unsigned char* src, Extent2D srcWh, Rect2D page, int channel)
    {
        auto offset = page.offset;
        auto extent = page.extent;

        uint32_t oriOffsetY = offset.y * srcWh.width * channel * ElementSize;
        uint32_t oriOffsetX = offset.x * channel * ElementSize;
        unsigned char* ori = src + oriOffsetY + oriOffsetX;

        LinearImage pageDst(extent.width, extent.height, channel, ElementSize);
        unsigned char* data = (unsigned char*)pageDst.GetData();
        uint32_t pixelSize = channel * ElementSize;
        uint32_t srcByteWidth = pixelSize * srcWh.width;
        uint32_t pageByteWidth = pixelSize * extent.width;

        for (int j = 0; j < extent.height; ++j)
        {
            memcpy(data + j * pageByteWidth, ori + j * srcByteWidth, pageByteWidth);
        }

        return pageDst;
    }
};
} // namespace Libs::Image
