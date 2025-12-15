#pragma once
#include "Engine/Library/Assert.hpp"
#include "Engine/Library/Math.hpp"

#include <cstdint>
#include "Engine/Library/DynamicArray.hpp"
class LinearCubemap
{
public:
    LinearCubemap(uint32_t width, uint32_t height, uint32_t channel, uint32_t elementSize, void* rawData)
        : channel(channel), width(width), height(height), elementSize(elementSize)
    {
        ASSERT(rawData && "rawData is needed");

        if (rawData)
        {
            singleFaceSize = height * width * channel * elementSize;
            totalSize = height * width * channel * elementSize * 6;

            data.resize(totalSize);
            memcpy(data.data(), rawData, totalSize);
        }
    }

    template <class T>
    T* Sample(float nx, float ny, int face)
    {
        int x = glm::clamp(nx * width + 0.5f, 0.f, width - 1.0f);
        int y = glm::clamp(ny * height + 0.5f, 0.f, height - 1.0f);

        ASSERT(sizeof(T) == elementSize);
        ASSERT(face >= 0 && face < 6);
        ASSERT(x >= 0 && x < width && "x coordinate out of bounds");
        ASSERT(y >= 0 && y < height && "y coordinate out of bounds");

        uint32_t index = (y * width + x) * channel * elementSize + face * singleFaceSize;

        ASSERT(index < totalSize && "index out of bounds");

        return reinterpret_cast<T*>(&data[index]);
    }

    template <class T>
    glm::vec<4, T, glm::highp> Sample4(float nx, float ny, int face)
    {
        int x = glm::clamp(nx * width + 0.5f, 0.f, width - 1.f);
        int y = glm::clamp(ny * height + 0.5f, 0.f, height - 1.f);

        ASSERT(elementSize == 4);
        ASSERT(face >= 0 && face < 6);
        ASSERT(x >= 0 && x < width && "x coordinate out of bounds");
        ASSERT(y >= 0 && y < height && "y coordinate out of bounds");

        uint32_t index = (y * width + x) * channel * elementSize + face * singleFaceSize;

        ASSERT(index < totalSize && "index out of bounds");

        return *reinterpret_cast<glm::vec<4, T, glm::highp>*>(&data[index]);
    }

    template <class T>
    glm::vec<4, T, glm::highp> Sample4(float3 dir)
    {
        int face = 0;
        float2 uv = DirToUV(dir, face);
        int x = glm::clamp(uv.x * width + 0.5f, 0.f, width - 1.f);
        int y = glm::clamp(uv.y * height + 0.5f, 0.f, height - 1.f);

        ASSERT(elementSize == 4);
        ASSERT(face >= 0 && face < 6);
        ASSERT(x >= 0 && x < width && "x coordinate out of bounds");
        ASSERT(y >= 0 && y < height && "y coordinate out of bounds");

        uint32_t index = (y * width + x) * channel * elementSize + face * singleFaceSize;

        ASSERT(index < totalSize && "index out of bounds");

        return *reinterpret_cast<glm::vec<4, T, glm::highp>*>(&data[index]);
    }

    static float2 DirToUV(float3 dir, int& face)
    {
        float absX = std::fabs(dir.x);
        float absY = std::fabs(dir.y);
        float absZ = std::fabs(dir.z);

        int faceIndex;
        float2 uv;

        if (absX >= absY && absX >= absZ)
        {
            if (dir.x > 0)
            {
                faceIndex = 0; // Positive X
                uv.x = -dir.z / absX;
                uv.y = -dir.y / absX;
            }
            else
            {
                faceIndex = 1; // Negative X
                uv.x = dir.z / absX;
                uv.y = -dir.y / absX;
            }
        }
        else if (absY >= absX && absY >= absZ)
        {
            if (dir.y > 0)
            {
                faceIndex = 2; // Positive Y
                uv.x = dir.x / absY;
                uv.y = dir.z / absY;
            }
            else
            {
                faceIndex = 3; // Negative Y
                uv.x = dir.x / absY;
                uv.y = -dir.z / absY;
            }
        }
        else
        {
            if (dir.z > 0)
            {
                faceIndex = 4; // Positive Z
                uv.x = dir.x / absZ;
                uv.y = -dir.y / absZ;
            }
            else
            {
                faceIndex = 5; // Negative Z
                uv.x = -dir.x / absZ;
                uv.y = -dir.y / absZ;
            }
        }

        face = faceIndex;
        return float2(uv.x, uv.y) * 0.5f + 0.5f;
    }

private:
    uint32_t height;
    uint32_t width;
    uint32_t channel;
    uint32_t elementSize;
    uint32_t totalSize;
    uint32_t singleFaceSize;
    std::vector<unsigned char> data;
};
