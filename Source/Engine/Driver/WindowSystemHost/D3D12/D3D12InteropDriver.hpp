#pragma once
#include "../IInteropDriver.hpp"
#include <d3d12.h>
#include <dxgi1_4.h>
#include <dcomp.h>
#include <wrl/client.h>

namespace WindowSystemHost
{
    class D3D12InteropDriver : public IInteropDriver
    {
    public:
        D3D12InteropDriver();
        ~D3D12InteropDriver() override;

        void Initialize(void* windowHandle, uint32_t width, uint32_t height) override;
        void Resize(uint32_t width, uint32_t height) override;
        void* GetSharedHandle() override;
        void* GetFenceHandle() override;
        void Wait(uint64_t value) override;
        void Signal(uint64_t value) override;
        void Present() override;

    private:
        void CreateDevice();
        void CreateSwapChain(uint32_t width, uint32_t height);
        void SetupDComp(HWND hwnd);
        void CreateSyncFence();

        Microsoft::WRL::ComPtr<ID3D12Device> m_device;
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
        
        Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
        Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
        
        Microsoft::WRL::ComPtr<IDCompositionDevice> m_dcompDevice;
        Microsoft::WRL::ComPtr<IDCompositionTarget> m_dcompTarget;
        Microsoft::WRL::ComPtr<IDCompositionVisual> m_dcompVisual;

        Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_intermediateTexture;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_swapChainBuffers[2];
        HANDLE m_sharedFenceHandle = nullptr;
        HANDLE m_sharedTextureHandle = nullptr;
    };
}
