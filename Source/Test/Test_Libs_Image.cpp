#pragma once
#include "Libs/Image/Processor.hpp"
#include "ThirdParty/stb/stb_image.h"
#include "ThirdParty/stb/stb_image_write.h"
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <memory>
TEST(Libs, Image)
{
    using namespace Libs::Image;

    std::filesystem::path srcFilePath(__FILE__);
    srcFilePath = srcFilePath.parent_path() / "Resources/rocky_trail_diff_1k.jpg";
    std::filesystem::path dstFilePath(__FILE__);
    dstFilePath = dstFilePath.parent_path() / "Resources/rocky_trail_diff_512kb.jpg";

    std::ifstream srcf(srcFilePath, std::ios::binary);
    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(srcf), {});

    int width, height, channels, desiredChannels;
    stbi_info_from_memory(buffer.data(), buffer.size(), &width, &height, &desiredChannels);

    if (desiredChannels == 3) desiredChannels = 4;

    stbi_uc* loaded = stbi_load_from_memory(buffer.data(), buffer.size(), &width, &height, &channels, desiredChannels);

    Processor imageProcessor;
    LinearImage src(width, height, desiredChannels);

    // convert byte to float
    for (int i = 0; i < src.GetElementCount(); ++i)
    {
        src.GetData()[i] = loaded[i] / (float)std::numeric_limits<stbi_uc>::max();
    }

    auto dst = imageProcessor.GenerateMipmap(&src, filterFn_Box, 1);
    std::unique_ptr<stbi_uc> output = std::unique_ptr<stbi_uc>(new stbi_uc[dst->GetElementCount()]);
    for (int i = 0; i < dst->GetElementCount(); ++i)
    {
        output.get()[i] = (unsigned char)std::round(dst->GetData()[i] * (float)std::numeric_limits<stbi_uc>::max());
    }

    stbi_write_jpg(dstFilePath.string().c_str(),
                   dst->GetWidth(),
                   dst->GetHeight(),
                   dst->GetChannel(),
                   (const void*)output.get(),
                   100);
}
