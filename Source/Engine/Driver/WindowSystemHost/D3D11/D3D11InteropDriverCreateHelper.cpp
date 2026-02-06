#include "D3D11InteropDriverCreateHelper.hpp"
#include "Engine/Driver/WindowSystemHost/D3D11/D3D11InteropDriver.hpp"

std::unique_ptr<WindowSystemHost::IInteropDriver> CreateD3D11InteropDriver()
{
    return std::make_unique<WindowSystemHost::D3D11InteropDriver>();
}
