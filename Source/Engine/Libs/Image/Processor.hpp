#pragma once
#include "Libs/Constants.hpp"
#include "Libs/Ptr.hpp"
#include "LinearImage.hpp"
#include <algorithm>
#include <cmath>
#include <functional>

namespace Libs::Image
{
/**
 * nomenclature:
 * s = source
 * t = target
 * i = integer row index
 * j = integer column index
 * x = normalized row pixel coord
 */
const float fPi = (float)Constants::Pi;
using FilterFn = float (*)(float t, float distance);
inline float filterFn_Gaussian(float t, float a)
{
    return 1 / (a * std::sqrt(2 * fPi)) * std::exp(-0.5f * ((t * t) / (a * a)));
};

inline float filterFn_Box(float t, float a) { return 1; }
class Processor
{
public:
    std::unique_ptr<LinearImage> GenerateMipmap(RefPtr<LinearImage> source, FilterFn filterFn, float sampleCount)
    {
        auto r0 = Transpose(RowDownSample(source, filterFn, sampleCount));
        auto r1 = Transpose(RowDownSample(r0, filterFn, sampleCount));

        return r1;
    }

    std::unique_ptr<LinearImage> Transpose(RefPtr<LinearImage> source);

    std::unique_ptr<LinearImage> RowDownSample(RefPtr<LinearImage> sImage,
                                               FilterFn filterFn,
                                               const uint32_t sampleDistance);
};
} // namespace Libs::Image
