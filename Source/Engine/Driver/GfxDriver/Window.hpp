#pragma once
#include "Image.hpp"
namespace Gfx
{
class Window
{
public:
    virtual ~Window() {}
    virtual Image* GetSwapchainImage() = 0;
    virtual void SetSurfaceSize(int width, int height) = 0;
    virtual void Present() = 0;
};
} // namespace Gfx
