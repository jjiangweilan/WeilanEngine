#pragma once
#include <cmath>

namespace Libs::Image
{

template <class T>
void GenerateBoxFilteredMipmap(
    uint8_t* source,
    int width,
    int height,
    int layers,
    int levels,
    int channels,
    uint8_t*& output,
    size_t& outputByteSize
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
    int perLayerElementCount = outputByteSize * channels;
    outputByteSize = layers * perLayerElementCount * sizeof(T);
    output = (uint8_t*)(T*)new uint8_t[outputByteSize];
    T* data = (T*)output;

    int layerOffset = 0;
    for (int layer = 0; layer < layers; ++layer)
    {
        int preLevelWidth = width;
        int preLevelHeight = height;
        int preLevelOffset = 0;
        int curLevelOffset = 0;
        layerOffset = layer * perLayerElementCount;

        size_t firstLevelByteSize = width * height * sizeof(T) * channels;
        memcpy(data + layerOffset, source + layer * firstLevelByteSize, firstLevelByteSize);

        for (int i = 1; i < levels; i++)
        {
            int lw = preLevelWidth * 0.5f;
            int lh = preLevelHeight * 0.5f;
            curLevelOffset = preLevelOffset + preLevelWidth * preLevelHeight * channels;

            // 2x2 box filter
            for (int k = 0; k < lh; k++)
            {
                for (int j = 0; j < lw; j++)
                {
                    for (int c = 0; c < channels; ++c)
                    {
                        int x = std::min(2 * j, preLevelWidth - 1);
                        int y = std::min(2 * k, preLevelHeight - 1);
                        int samples = 1;

                        T sum = data[layerOffset + preLevelOffset + (y * preLevelWidth + x) * channels + c];
                        if (y + 1 < preLevelHeight)
                        {
                            sum += data[layerOffset + preLevelOffset + ((y + 1) * preLevelWidth + x) * channels + c];
                            samples += 1;
                        }
                        if (x + 1 < preLevelWidth)
                        {
                            sum += data[layerOffset + preLevelOffset + (y * preLevelWidth + (x + 1)) * channels + c];
                            samples += 1;
                        }
                        if (x + 1 < preLevelWidth && y + 1 < preLevelHeight)
                        {
                            sum +=
                                data[layerOffset + preLevelOffset + ((y + 1) * preLevelWidth + (x + 1)) * channels + c];
                            samples += 1;
                        }

                        data[layerOffset + curLevelOffset + (k * lw + j) * channels + c] = sum / samples;
                    }
                }
            }

            preLevelOffset = curLevelOffset;
            preLevelWidth = lw;
            preLevelHeight = lh;
        }
    }
}

// expecting source to be a cubemap with x,-x,y,-y,z,-z
void GenerateIrradianceCubemap(float* source, int width, int height, int outputSize, uint8_t*& output);
} // namespace Libs::Image
