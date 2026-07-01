// this header is used to help creating the D3D11InteropDriver because windows API occupys class name like UUID
#pragma once
#include "../IInteropDriver.hpp"
#include <cstdint>
#include <memory>

std::unique_ptr<WindowSystemHost::IInteropDriver> CreateD3D11InteropDriver();

void* WeilanEngine_CreateWindow(int32_t x, int32_t y, uint32_t width, uint32_t height);
void WeilanEngine_SetWindowBounds(void* windowHandle, int32_t x, int32_t y, uint32_t width, uint32_t height);
void WeilanEngine_DestroyWindow(void* windowHandle);
void WeilanEngine_SetWindowHitTestDriver(void* windowHandle, WindowSystemHost::IInteropDriver* driver);
