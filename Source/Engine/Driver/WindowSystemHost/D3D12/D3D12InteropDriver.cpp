#include "D3D12InteropDriver.hpp"
#include <d3d11.h>
#include <dwmapi.h>
#include <iostream>
#include <stdexcept>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dcomp.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")

using namespace Microsoft::WRL;

namespace WindowSystemHost
{
D3D12InteropDriver::D3D12InteropDriver() = default;
D3D12InteropDriver::~D3D12InteropDriver()
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

void D3D12InteropDriver::Initialize(void* windowHandle, uint32_t width, uint32_t height)
{
    HWND hwnd = static_cast<HWND>(windowHandle);

    // 1. Window Setup
    LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP;
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);

    DWM_BLURBEHIND bb = {0};
    bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
    bb.fEnable = TRUE;
    bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
    DwmEnableBlurBehindWindow(hwnd, &bb);

    // 2. Create Device & Dependencies
    CreateDevice();
    SetupDComp(hwnd);
    CreateSyncFence();

    // 3. Initial Resize
    Resize(width, height);
}

void D3D12InteropDriver::CreateDevice()
{
    UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_dxgiFactory));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create DXGI Factory");

    hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create D3D12 Device");

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    hr = m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create Command Queue");

    hr = m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create Command Allocator");

    hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create Command List");
    
    m_commandList->Close();
}

void D3D12InteropDriver::SetupDComp(HWND hwnd)
{
    // For D3D12, we can use DCompositionCreateDevice with nullptr to use the default device,
    // or we can use a D3D11 device. DirectComposition doesn't support D3D12 devices directly 
    // in the old DCompositionCreateDevice API. 
    // However, we can use IDCompositionDevice2 or just use D3D11 for composition.
    // Given D3D11 is already there, using D3D11 for composition is safer.
    
    ComPtr<ID3D11Device> d3d11Device;
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &d3d11Device, nullptr, nullptr);
    
    ComPtr<IDXGIDevice> dxgiDevice;
    d3d11Device.As(&dxgiDevice);

    HRESULT hr = DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&m_dcompDevice));
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

void D3D12InteropDriver::CreateSyncFence()
{
    HRESULT hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_SHARED, IID_PPV_ARGS(&m_fence));
    if (FAILED(hr))
        throw std::runtime_error("Failed to create ID3D12Fence");

    hr = m_device->CreateSharedHandle(m_fence.Get(), nullptr, GENERIC_ALL, nullptr, &m_sharedFenceHandle);
    if (FAILED(hr))
        throw std::runtime_error("Failed to create shared fence handle");
}

void D3D12InteropDriver::Resize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
        return;

    // Cleanup
    if (m_sharedTextureHandle)
    {
        CloseHandle(m_sharedTextureHandle);
        m_sharedTextureHandle = nullptr;
    }
    m_intermediateTexture.Reset();
    for (int i = 0; i < 2; ++i) m_swapChainBuffers[i].Reset();

    if (m_swapChain)
    {
        HRESULT hr = m_swapChain->ResizeBuffers(2, width, height, DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
        if (FAILED(hr))
            throw std::runtime_error("Failed to resize swapchain");
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

        ComPtr<IDXGISwapChain1> swapChain1;
        HRESULT hr = m_dxgiFactory->CreateSwapChainForComposition(
            m_commandQueue.Get(),
            &desc,
            nullptr,
            &swapChain1
        );
        if (FAILED(hr))
            throw std::runtime_error("Failed to create SwapChain for Composition");

        swapChain1.As(&m_swapChain);

        m_dcompVisual->SetContent(m_swapChain.Get());
        m_dcompDevice->Commit();
    }

    for (UINT i = 0; i < 2; i++)
    {
        m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_swapChainBuffers[i]));
    }

    // Create Intermediate Shared Texture
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_SHARED,
        &texDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&m_intermediateTexture)
    );
    if (FAILED(hr))
        throw std::runtime_error("Failed to create intermediate shared texture");

    hr = m_device->CreateSharedHandle(m_intermediateTexture.Get(), nullptr, GENERIC_ALL, nullptr, &m_sharedTextureHandle);
    if (FAILED(hr))
        throw std::runtime_error("Failed to create shared texture handle");
}

void* D3D12InteropDriver::GetSharedHandle()
{
    return m_sharedTextureHandle;
}

void* D3D12InteropDriver::GetFenceHandle()
{
    return m_sharedFenceHandle;
}

void D3D12InteropDriver::Wait(uint64_t value)
{
    m_commandQueue->Wait(m_fence.Get(), value);
}

void D3D12InteropDriver::Signal(uint64_t value)
{
    m_commandQueue->Signal(m_fence.Get(), value);
}

void D3D12InteropDriver::Present()
{
    if (m_intermediateTexture && m_swapChain)
    {
        UINT backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
        
        m_commandAllocator->Reset();
        m_commandList->Reset(m_commandAllocator.Get(), nullptr);

        D3D12_RESOURCE_BARRIER barriers[2] = {};
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource = m_intermediateTexture.Get();
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COMMON;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        
        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource = m_swapChainBuffers[backBufferIndex].Get();
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

        m_commandList->ResourceBarrier(2, barriers);
        m_commandList->CopyResource(m_swapChainBuffers[backBufferIndex].Get(), m_intermediateTexture.Get());

        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COMMON;
        
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

        m_commandList->ResourceBarrier(2, barriers);
        m_commandList->Close();

        ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(1, ppCommandLists);

        m_swapChain->Present(1, 0);
        m_dcompDevice->Commit();
        
        // Simple flush to ensure command list is finished before next frame's reset.
        ComPtr<ID3D12Fence> flushFence;
        m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&flushFence));
        m_commandQueue->Signal(flushFence.Get(), 1);
        HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        flushFence->SetEventOnCompletion(1, event);
        WaitForSingleObject(event, INFINITE);
        CloseHandle(event);
    }
}
} // namespace WindowSystemHost
