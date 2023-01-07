#pragma once
#include <cinttypes>
#include <memory>

namespace Libs::Image
{
class LinearImage
{
public:
    LinearImage(uint32_t width, uint32_t height, uint32_t channel) : channel(channel), width(width), height(height)
    {
        data = std::unique_ptr<float>(new float[width * height * channel]);
        memset(data.get(), 0, width * height * channel * sizeof(float));
    }
    LinearImage(const LinearImage& other) = delete;

    float* GetData() const { return data.get(); }
    uint32_t GetWidth() const { return width; }
    uint32_t GetHeight() const { return height; }
    uint32_t GetChannel() const { return channel; }
    uint32_t GetSize() const { return width * height * channel * sizeof(float); }
    uint32_t GetElementCount() const { return width * height * channel; }

private:
    std::unique_ptr<float> data;
    uint32_t channel;
    uint32_t width;
    uint32_t height;
};
} // namespace Libs::Image
