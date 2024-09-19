#pragma once
#include <cmath>

namespace Libs::Image
{

template <class T>
void GenerateBoxFilteredMipmap(
    uint8_t* source, int width, int height, int levels, int channels, uint8_t*& output, size_t& outputByteSize
)
{
    outputByteSize = 0;
    for (int i = 0; i < levels; i++)
    {
        float scale = std::pow(0.5f, i);
        int lw = width * scale;
        int lh = height * scale;

        outputByteSize += lw * lh;
    }
    outputByteSize = outputByteSize * channels * sizeof(T);
    output = (uint8_t*)(T*)new uint8_t[outputByteSize];
    T* data = (T*)output;

    size_t firstLevelByteSize = width * height * sizeof(T) * channels;
    memcpy(data, source, firstLevelByteSize);

    int preLevelWidth = width;
    int preLevelHeight = height;
    int preLevelOffset = 0;
    int curLevelOffset = 0;
    for (int i = 1; i < levels; i++)
    {
        float scale = std::pow(0.5f, i);
        int lw = preLevelWidth * scale;
        int lh = preLevelHeight * scale;
        curLevelOffset = preLevelOffset + preLevelWidth * preLevelHeight * channels;

        // 2x2 box filter
        for (int k = 0; k < lh; k++)
        {
            for (int j = 0; j < lw; j++)
            {
                for (int c = 0; c < channels; ++c)
                {
                    T val0 = data
                        [preLevelOffset + std::min(2 * k, preLevelHeight - 1) * preLevelWidth * channels +
                         std::min(2 * j, preLevelWidth - 1) * channels + c];
                    T val1 = data
                        [preLevelOffset + std::min(2 * (k + 1), preLevelHeight - 1) * preLevelWidth * channels +
                         std::min(2 * j, preLevelWidth - 1) * channels + c];
                    T val2 = data
                        [preLevelOffset + std::min(2 * k, preLevelHeight - 1) * preLevelWidth * channels +
                         std::min(2 * (j + 1), preLevelWidth - 1) * channels + c];
                    T val3 = data
                        [preLevelOffset + std::min(2 * (k + 1), preLevelHeight - 1) * preLevelWidth * channels +
                         std::min(2 * (j + 1), preLevelWidth - 1) * channels + c];
                    data[curLevelOffset + k * lw * channels + j * channels + c] = (val0 + val1 + val2 + val3) / 4;
                }
            }
        }

        preLevelOffset = curLevelOffset;
        preLevelWidth = lw;
        preLevelHeight = lh;
    }
}

// expecting source to be a cubemap with x,-x,y,-y,z,-z
void GenerateIrradianceCubemap(float* source, int width, int height, float*& output);
} // namespace Libs::Image
