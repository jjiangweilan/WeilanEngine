#pragma once
#include "Libs/Assert.hpp"
#include "Libs/Math.hpp"

#include <cstdint>
#include <vector>
class LinearCubemap
{
public:
    LinearCubemap(uint32_t width, uint32_t height, uint32_t channel, uint32_t elementSize, void* rawData)
        : channel(channel), width(width), height(height), elementSize(elementSize)
    {
        ASSERT(rawData && "rawData is needed");

        if (rawData)
        {
            totalSize = height * width * channel * elementSize;

            data.resize(totalSize);
            memcpy(data.data(), rawData, totalSize);
        }
    }

    template <class T>
    T* Sample(int x, int y)
    {
        ASSERT(sizeof(T) == elementSize);
        ASSERT(x >= 0 && x < width && "x coordinate out of bounds");
        ASSERT(y >= 0 && y < height && "y coordinate out of bounds");

        uint32_t index = (y * width + x) * channel * elementSize;

        ASSERT(index < totalSize && "index out of bounds");

        return reinterpret_cast<T*>(&data[index]);
    }

    template <class T>
    glm::vec<4, T, glm::highp> Sample4(int x, int y)
    {
        ASSERT(elementSize == 4);
        ASSERT(x >= 0 && x < width && "x coordinate out of bounds");
        ASSERT(y >= 0 && y < height && "y coordinate out of bounds");

        uint32_t index = (y * width + x) * channel * elementSize;

        ASSERT(index < totalSize && "index out of bounds");

        return *reinterpret_cast<glm::vec<4, T, glm::highp>*>(&data[index]);
    }

private:
    uint32_t height;
    uint32_t width;
    uint32_t channel;
    uint32_t elementSize;
    uint32_t totalSize;
    std::vector<unsigned char> data;
};
