#pragma once
#include "../IInteropDriver.hpp"
#include <d3d11_4.h>
#include <dcomp.h>
#include <dxgi1_3.h>
#include <wrl/client.h>

namespace WindowSystemHost
{
class D3D11InteropDriver : public IInteropDriver
{
public:
    D3D11InteropDriver();
    ~D3D11InteropDriver() override;

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

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<ID3D11Device5> m_device5;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext4> m_context4;

    Microsoft::WRL::ComPtr<IDXGIFactory2> m_dxgiFactory;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swapChain;

    Microsoft::WRL::ComPtr<IDCompositionDevice> m_dcompDevice;
    Microsoft::WRL::ComPtr<IDCompositionTarget> m_dcompTarget;
    Microsoft::WRL::ComPtr<IDCompositionVisual> m_dcompVisual;

    Microsoft::WRL::ComPtr<ID3D11Fence> m_fence;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_intermediateTexture;
    ID3D11RenderTargetView* pRTV = nullptr;
    HANDLE m_sharedFenceHandle = nullptr;
    HANDLE m_sharedTextureHandle = nullptr;
};
} // namespace WindowSystemHost
