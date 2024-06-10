#pragma once
#include "Image.hpp"

namespace Gfx
{
class SwapchainImage
{
    virtual Image* GetActiveImage() = 0;
};
} // namespace Gfx
