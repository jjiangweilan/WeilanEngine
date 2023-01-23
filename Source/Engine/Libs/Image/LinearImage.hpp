#pragma once
#include <cinttypes>
#include <memory>

namespace Libs::Image
{
class LinearImage
{
public:
    LinearImage(
        uint32_t width, uint32_t height, uint32_t channel, uint32_t elementSize, unsigned char* externalData = nullptr)
        : channel(channel), width(width), height(height), elementSize(elementSize)
    {
        if (externalData)
        {
            data = std::unique_ptr<unsigned char>(externalData);
        }
        else
        {
            data = std::unique_ptr<unsigned char>(new unsigned char[width * height * channel * elementSize]);
            memset(data.get(), 0, width * height * channel * elementSize);
        }
    }
    LinearImage(LinearImage&& other)
        : data(std::exchange(other.data, nullptr)), channel(other.channel), width(other.width), height(other.height),
          elementSize(other.elementSize)
    {}
    LinearImage(const LinearImage& other) = delete;

    unsigned char* GetData() const { return data.get(); }
    uint32_t GetWidth() const { return width; }
    uint32_t GetHeight() const { return height; }
    uint32_t GetPixelCount() const { return width * height; }
    uint32_t GetChannel() const { return channel; }
    uint32_t GetSize() const { return width * height * channel * elementSize; }
    uint32_t GetElementCount() const { return width * height * channel; }
    uint32_t GetElementSize() const { return elementSize; }

private:
    std::unique_ptr<unsigned char> data;
    uint32_t channel;
    uint32_t width;
    uint32_t height;
    uint32_t elementSize;
};
} // namespace Libs::Image
