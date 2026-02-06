#pragma once
#include "../IInteropDriver.hpp"
#include <memory>

namespace WindowSystemHost
{
    std::unique_ptr<WindowSystemHost::IInteropDriver> CreateD3D12InteropDriver();
}
