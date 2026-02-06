#include "D3D12InteropDriverCreateHelper.hpp"
#include "D3D12InteropDriver.hpp"

namespace WindowSystemHost
{
std::unique_ptr<WindowSystemHost::IInteropDriver> CreateD3D12InteropDriver()
{
    return std::make_unique<WindowSystemHost::D3D12InteropDriver>();
}
}
