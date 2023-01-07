#include "Processor.hpp"
#include <iostream>
namespace Libs::Image
{
std::unique_ptr<LinearImage> Processor::RowDownSample(RefPtr<LinearImage> sImage,
                                                      FilterFn filterFn,
                                                      const uint32_t sampleCount)
{
    const auto sWidth = sImage->GetWidth();
    const auto sHeight = sImage->GetHeight();
    const auto channel = sImage->GetChannel();
    auto tImage = std::make_unique<LinearImage>(sWidth / 2, sHeight, channel);
    const auto tWidth = tImage->GetWidth();

    float* tData = tImage->GetData();
    float* sData = sImage->GetData();
    float sxDelta = 1.0f / sWidth;
    for (uint32_t tj = 0; tj < sHeight; ++tj)
    {
        for (uint32_t ti = 0; ti < tWidth; ++ti)
        {
            uint32_t pixelIndex = ti + tj * tWidth;
            float tx = (ti + 0.5) / (float)tWidth;
            float siCenter = tx * sWidth;
            float totalWeight = 0;
            for (float s = -(int)sampleCount / 2.0f; s < sampleCount / 2.0f; s += 1.0f)
            {
                float siSample = std::clamp(std::floorf((tx + sxDelta * s) * sWidth - 0.5), 0.0f, (float)sWidth);

                float weight = filterFn(siSample - siCenter, sampleCount);
                totalWeight += weight;
                for (uint32_t n = 0; n < channel; ++n)
                {
                    tData[pixelIndex * channel + n] +=
                        sData[(uint32_t)siSample * channel + n + tj * sWidth * channel] * weight;
                }
            }
            for (uint32_t n = 0; n < channel; ++n)
            {
                tData[pixelIndex * channel + n] /= totalWeight;
                assert(!isnan(tData[pixelIndex * channel + n]));
            }
        }
    }

    return tImage;
}

std::unique_ptr<LinearImage> Processor::Transpose(RefPtr<LinearImage> source)
{
    auto dst = std::make_unique<LinearImage>(source->GetHeight(), source->GetWidth(), source->GetChannel());

    float* s = source->GetData();
    float* d = dst->GetData();

    const uint32_t sWidth = source->GetWidth();
    const uint32_t sHeight = source->GetHeight();
    const uint32_t sChannel = source->GetChannel();
    for (uint32_t i = 0; i < sWidth; ++i)
    {
        for (uint32_t j = 0; j < sHeight; ++j)
        {
            for (uint32_t n = 0; n < sChannel; ++n)
            {
                d[i * sHeight * sChannel + j * sChannel + n] = s[i * sChannel + j * sWidth * sChannel + n];
            }
        }
    }

    return dst;
}
} // namespace Libs::Image
