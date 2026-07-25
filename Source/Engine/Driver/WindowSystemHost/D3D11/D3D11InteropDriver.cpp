#include "D3D11InteropDriver.hpp"
#include <SDL.h>
#include <SDL_syswm.h>
#include <algorithm>
#include <dwmapi.h>
#include <iostream>
#include <stdexcept>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")

using namespace Microsoft::WRL;

namespace WindowSystemHost
{
D3D11InteropDriver::D3D11InteropDriver() = default;
D3D11InteropDriver::~D3D11InteropDriver()
{
    if (m_sharedFenceHandle)
    {
        CloseHandle(m_sharedFenceHandle);
        m_sharedFenceHandle = nullptr;
    }
    if (m_sharedTextureHandle)
    {
        CloseHandle(m_sharedTextureHandle);
        m_sharedTextureHandle = nullptr;
    }
}

void D3D11InteropDriver::Initialize(void* windowHandle, uint32_t width, uint32_t height)
{
    HWND hwnd = (HWND)windowHandle;
    if (!hwnd)
    {
        throw std::runtime_error("Failed to get HWND from SDL window");
    }
    m_hwnd = hwnd;

    CreateDevice();
    SetupDComp(hwnd);
    CreateSyncFence();

    Resize(width, height);
}

void D3D11InteropDriver::CreateDevice()
{
    UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
    D3D_FEATURE_LEVEL createdFeatureLevel;

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevels,
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION,
        &m_device,
        &createdFeatureLevel,
        &m_context
    );

    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create D3D11 Device");
    }

    hr = m_device.As(&m_device5);
    if (FAILED(hr))
        throw std::runtime_error("Failed to query ID3D11Device5");

    hr = m_context.As(&m_context4);
    if (FAILED(hr))
        throw std::runtime_error("Failed to query ID3D11DeviceContext4");

    hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&m_dxgiFactory));
    if (FAILED(hr))
    {
        // Fallback to factory1 if factory2 fails
        hr = CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory));
        if (FAILED(hr))
            throw std::runtime_error("Failed to create DXGI Factory");
    }
}

void D3D11InteropDriver::SetupDComp(HWND hwnd)
{
    HRESULT hr = DCompositionCreateDevice(nullptr, IID_PPV_ARGS(&m_dcompDevice));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create DComposition Device");

    hr = m_dcompDevice->CreateTargetForHwnd(hwnd, TRUE, &m_dcompTarget);
    if (FAILED(hr))
        throw std::runtime_error("Failed to create DComposition Target");

    hr = m_dcompDevice->CreateVisual(&m_dcompVisual);
    if (FAILED(hr))
        throw std::runtime_error("Failed to create DComposition Visual");

    hr = m_dcompTarget->SetRoot(m_dcompVisual.Get());
    if (FAILED(hr))
        throw std::runtime_error("Failed to set DComposition Root");
}

void D3D11InteropDriver::CreateSyncFence()
{
    HRESULT hr = m_device5->CreateFence(0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create ID3D11Fence");

    hr = m_fence->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr, &m_sharedFenceHandle);
    if (FAILED(hr))
        throw std::runtime_error("Failed to create shared fence handle");
}

void D3D11InteropDriver::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    m_width = width;
    m_height = height;
    m_hasHitTestCache = false;

    // Cleanup old shared handle and texture
    if (m_sharedTextureHandle)
    {
        CloseHandle(m_sharedTextureHandle);
        m_sharedTextureHandle = nullptr;
    }
    m_intermediateTexture.Reset();

    // 1. Resize/Create SwapChain
    if (m_swapChain)
    {
        HRESULT hr = m_swapChain->ResizeBuffers(2, width, height, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
        if (FAILED(hr))
        {
            throw std::runtime_error("Failed to resize swapchain");
        }
    }
    else
    {
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.Stereo = FALSE;
        desc.SampleDesc.Count = 1;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
        desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

        ComPtr<IDXGIDevice> dxgiDevice;
        HRESULT hr = m_device.As(&dxgiDevice);
        if (FAILED(hr))
            throw std::runtime_error("Failed to query IDXGIDevice for composition swapchain");

        hr = m_dxgiFactory->CreateSwapChainForComposition(
            dxgiDevice.Get(),
            &desc,
            nullptr,
            &m_swapChain
        );
        if (FAILED(hr))
            throw std::runtime_error("Failed to create SwapChain for Composition");

        hr = m_dcompVisual->SetContent(m_swapChain.Get());
        if (FAILED(hr))
            throw std::runtime_error("Failed to set DComposition visual content");

        hr = m_dcompDevice->Commit();
        if (FAILED(hr))
            throw std::runtime_error("Failed to commit DComposition swapchain setup");
    }

    // 2. Create Intermediate Shared Texture
    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags = 0;
    texDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;

    HRESULT hr = m_device->CreateTexture2D(&texDesc, nullptr, &m_intermediateTexture);
    if (FAILED(hr))
        throw std::runtime_error("Failed to create intermediate shared texture");

    ComPtr<IDXGIResource1> sharedResource;
    hr = m_intermediateTexture.As(&sharedResource);
    if (FAILED(hr))
        throw std::runtime_error("Failed to QI IDXGIResource1");

    hr = sharedResource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &m_sharedTextureHandle);
    if (FAILED(hr))
        throw std::runtime_error("Failed to create shared texture handle");

    CreateHitTestStagingTextures();
}

void D3D11InteropDriver::SetWindowPosition(int32_t x, int32_t y)
{
    SetWindowPos(m_hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

WindowPosition D3D11InteropDriver::GetWindowPosition() const
{
    RECT windowRect{};
    GetWindowRect(m_hwnd, &windowRect);
    return {windowRect.left, windowRect.top};
}

void D3D11InteropDriver::CreateHitTestStagingTextures()
{
    for (auto& readback : m_hitTestReadbacks)
    {
        readback.texture.Reset();
        readback.pending = false;

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = HitTestSampleSize;
        desc.Height = HitTestSampleSize;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        HRESULT hr = m_device->CreateTexture2D(&desc, nullptr, &readback.texture);
        if (FAILED(hr))
            throw std::runtime_error("Failed to create D3D11 hit-test staging texture");
    }

    m_hitTestReadbackIndex = 0;
}

void D3D11InteropDriver::UpdateHitTestCache()
{
    if (!m_hwnd || !m_intermediateTexture || m_width == 0 || m_height == 0)
        return;

    auto& readback = m_hitTestReadbacks[m_hitTestReadbackIndex];
    if (readback.pending && readback.texture)
    {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        HRESULT hr = m_context->Map(readback.texture.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
        if (SUCCEEDED(hr))
        {
            for (int y = 0; y < readback.height; ++y)
            {
                auto* row = static_cast<const uint8_t*>(mapped.pData) + y * mapped.RowPitch;
                for (int x = 0; x < readback.width; ++x)
                {
                    m_hitTestAlphaCache[y * HitTestSampleSize + x] = row[x * 4 + 3];
                }
            }
            m_context->Unmap(readback.texture.Get(), 0);

            m_hitTestCacheX = readback.x;
            m_hitTestCacheY = readback.y;
            m_hitTestCacheWidth = readback.width;
            m_hitTestCacheHeight = readback.height;
            m_hasHitTestCache = true;
            readback.pending = false;
        }
        else if (hr != DXGI_ERROR_WAS_STILL_DRAWING)
        {
            readback.pending = false;
        }
    }
    if (readback.pending)
        return;

    POINT cursorPos{};
    if (!GetCursorPos(&cursorPos))
        return;
    if (!ScreenToClient(m_hwnd, &cursorPos))
        return;

    int cursorX = static_cast<int>(cursorPos.x);
    int cursorY = static_cast<int>(cursorPos.y);
    if (cursorX < 0 || cursorY < 0 || cursorX >= static_cast<int>(m_width) || cursorY >= static_cast<int>(m_height))
        return;

    int copyWidth = (std::min)(HitTestSampleSize, static_cast<int>(m_width));
    int copyHeight = (std::min)(HitTestSampleSize, static_cast<int>(m_height));
    int sourceX = (std::min)((std::max)(cursorX - copyWidth / 2, 0), static_cast<int>(m_width) - copyWidth);
    int sourceY = (std::min)((std::max)(cursorY - copyHeight / 2, 0), static_cast<int>(m_height) - copyHeight);

    D3D11_BOX sourceBox{};
    sourceBox.left = static_cast<UINT>(sourceX);
    sourceBox.top = static_cast<UINT>(sourceY);
    sourceBox.front = 0;
    sourceBox.right = static_cast<UINT>(sourceX + copyWidth);
    sourceBox.bottom = static_cast<UINT>(sourceY + copyHeight);
    sourceBox.back = 1;

    m_context->CopySubresourceRegion(readback.texture.Get(), 0, 0, 0, 0, m_intermediateTexture.Get(), 0, &sourceBox);
    readback.pending = true;
    readback.x = sourceX;
    readback.y = sourceY;
    readback.width = copyWidth;
    readback.height = copyHeight;

    m_hitTestReadbackIndex = (m_hitTestReadbackIndex + 1) % m_hitTestReadbacks.size();
}

bool D3D11InteropDriver::HitTestVisible(int clientX, int clientY) const
{
    if (!m_hasHitTestCache)
        return true;

    int localX = clientX - m_hitTestCacheX;
    int localY = clientY - m_hitTestCacheY;
    if (localX < 0 || localY < 0 || localX >= m_hitTestCacheWidth || localY >= m_hitTestCacheHeight)
        return true;

    int minX = (std::max)(localX - 1, 0);
    int minY = (std::max)(localY - 1, 0);
    int maxX = (std::min)(localX + 1, m_hitTestCacheWidth - 1);
    int maxY = (std::min)(localY + 1, m_hitTestCacheHeight - 1);
    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            if (m_hitTestAlphaCache[y * HitTestSampleSize + x] >= HitTestAlphaThreshold)
                return true;
        }
    }

    return false;
}

bool D3D11InteropDriver::IsCursorOverVisiblePixel() const
{
    if (!m_hwnd)
        return true;

    POINT cursorPos{};
    if (!GetCursorPos(&cursorPos))
        return true;
    if (!ScreenToClient(m_hwnd, &cursorPos))
        return true;

    int cursorX = static_cast<int>(cursorPos.x);
    int cursorY = static_cast<int>(cursorPos.y);
    if (cursorX < 0 || cursorY < 0 || cursorX >= static_cast<int>(m_width) || cursorY >= static_cast<int>(m_height))
        return true;

    return HitTestVisible(cursorX, cursorY);
}

void D3D11InteropDriver::SetWindowClickThrough(bool enable)
{
    if (!m_hwnd || m_windowClickThroughEnabled == enable)
        return;

    LONG_PTR exStyle = GetWindowLongPtr(m_hwnd, GWL_EXSTYLE);
    if (enable)
        exStyle |= WS_EX_TRANSPARENT;
    else
        exStyle &= ~WS_EX_TRANSPARENT;

    SetWindowLongPtr(m_hwnd, GWL_EXSTYLE, exStyle);
    SetWindowPos(
        m_hwnd,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED
    );
    m_windowClickThroughEnabled = enable;
}

void D3D11InteropDriver::CreateSwapChain(uint32_t width, uint32_t height)
{
    // Internal helper possibly not used if Resize handles everything
}

void* D3D11InteropDriver::GetSharedHandle()
{
    return m_sharedTextureHandle;
}

void* D3D11InteropDriver::GetFenceHandle()
{
    return m_sharedFenceHandle;
}

void D3D11InteropDriver::Wait(uint64_t value)
{
    if (m_context4 && m_fence)
    {
        m_context4->Wait(m_fence.Get(), value);
    }
}

void D3D11InteropDriver::Signal(uint64_t value)
{
    if (m_context4 && m_fence)
    {
        m_context4->Signal(m_fence.Get(), value);
    }
}

void D3D11InteropDriver::Present()
{
    if (m_intermediateTexture && m_swapChain)
    {
        UpdateHitTestCache();
        SetWindowClickThrough(!IsCursorOverVisiblePixel());

        ComPtr<ID3D11Texture2D> backBuffer;
        HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
        if (SUCCEEDED(hr))
        {
            m_context->CopyResource(backBuffer.Get(), m_intermediateTexture.Get());
            hr = m_swapChain->Present(1, 0);
            if (FAILED(hr))
                throw std::runtime_error("Failed to present D3D11 composition swapchain");

            hr = m_dcompDevice->Commit();
            if (FAILED(hr))
                throw std::runtime_error("Failed to commit DComposition presentation");
        }
        else
        {
            throw std::runtime_error("Failed to get D3D11 composition swapchain back buffer");
        }
    }
}
} // namespace WindowSystemHost
