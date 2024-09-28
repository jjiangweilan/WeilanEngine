#pragma once
#include "glm/ext/scalar_constants.hpp"
#include <cmath>
#include <glm/glm.hpp>

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

void GenerateIrradianceCubemap(float* source, int width, int height, int outputSize, uint8_t*& output);
void GenerateReflectanceCubemap(float* source, int width, int height, int outputSize, uint8_t*& output);

glm::vec3 GetDirFromCubeUV(glm::vec2 uv, int faceIndex);
glm::vec2 DirToEquirectangularUV(glm::vec3 dir);

template <class T>
void ConverToCubemap(T* source, int width, int height, int outputSize, int channels, uint8_t*& output)
{
    output = new uint8_t[channels * outputSize * outputSize * sizeof(T) * 6];
    T* out = (T*)(output);

    for (int layer = 0; layer < 6; ++layer)
    {
        int layerOffset = outputSize * outputSize * layer * channels;
        for (int y = 0; y < outputSize; ++y)
        {
            for (int x = 0; x < outputSize; ++x)
            {
                glm::vec3 dir =
                    glm::normalize(GetDirFromCubeUV({(x + 0.5) / outputSize, (y + 0.5) / outputSize}, layer));
                glm::vec2 uv = DirToEquirectangularUV(dir);
                glm::ivec2 pcoord = uv * glm::vec2(width, height);
                for (int c = 0; c < channels; ++c)
                {
                    out[layerOffset + (x + y * outputSize) * channels + c] =
                        source[(pcoord.x + pcoord.y * width) * channels + c];
                }
            }
        }
    }
}
} // namespace Libs::Image
