#pragma once
#include "Core/AssetObject.hpp"
#include "GfxDriver/Image.hpp"
#include "Libs/Image/LinearImage.hpp"
#include "Libs/Ptr.hpp"
#include "Utils/Structs.hpp"
#include <cinttypes>
#include <filesystem>

namespace Engine::Rendering
{
/**
 * capacity
 */
class VirtualTexture
{
public:
    VirtualTexture(const std::filesystem::path& vtFolder, uint32_t width, uint32_t height, uint32_t channel);

    uint32_t GetWidth() const { return info.width; }
    uint32_t GetHeight() const { return info.height; }
    uint32_t GetPixelCount() const { return info.pixelCount; }
    uint32_t GetByteSize() const { return info.byteSize; }
    uint32_t GetChannel() const { return info.channel; }
    Libs::Image::LinearImage Read(int x, int y, int mip, int desieredChannel);

private:
    const struct Info
    {
        uint32_t width;
        uint32_t height;

        uint32_t channel;
        uint32_t channelByteSize;
        uint32_t byteSize;
        uint32_t pixelCount;
    } info;

    std::filesystem::path vtFolder;

    Info GenInfo(const std::filesystem::path& vtFolder, uint32_t width, uint32_t height, uint32_t channel);
};
} // namespace Engine::Rendering
