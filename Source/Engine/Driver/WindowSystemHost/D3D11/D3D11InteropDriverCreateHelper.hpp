// this header is used to help creating the D3D11InteropDriver because windows API occupys class name like UUID
#pragma once
#include "../IInteropDriver.hpp"
#include <memory>

std::unique_ptr<WindowSystemHost::IInteropDriver> CreateD3D11InteropDriver();

void* WeilanEngine_CreateWindow();
