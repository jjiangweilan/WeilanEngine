#include "VirtualTexture.hpp"

#include "ThirdParty/stb/stb_image.h"
#include "spdlog/spdlog.h"
namespace Rendering
{
VirtualTexture::VirtualTexture(const std::filesystem::path& vtFolder, uint32_t width, uint32_t height, uint32_t channel)
    : info(GenInfo(vtFolder, width, height, channel)), vtFolder(vtFolder)
{}

VirtualTexture::Info VirtualTexture::GenInfo(
    const std::filesystem::path& vtFolder, uint32_t width, uint32_t height, uint32_t channel
)
{
    Info info;

    info.width = width;
    info.height = height;
    info.channel = channel;
    info.channelByteSize = 1; // no hdr support, just use 1 byte per channel
    info.pixelCount = width * height;
    info.byteSize = width * height * channel * info.channelByteSize;
    return info;
}

Libs::Image::LinearImage VirtualTexture::Read(int x, int y, int mip, int desieredChannel)
{
    auto file = fmt::format("{}_{}_{}.jpg", mip, x, y);
    auto path = vtFolder / file;

    int w, h, c;
    unsigned char* data = stbi_load(path.string().c_str(), &w, &h, &c, desieredChannel);
    return Libs::Image::LinearImage(w, h, 3, 1, data);
}
} // namespace Rendering
