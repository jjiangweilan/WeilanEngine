#pragma once
#include "../IInteropDriver.hpp"
#include <d3d11_4.h>
#include <dcomp.h>
#include <dxgi1_3.h>
#include <array>
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
    bool HitTestVisible(int clientX, int clientY) const override;

private:
    struct HitTestReadback
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        bool pending = false;
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

    void CreateDevice();
    void CreateSwapChain(uint32_t width, uint32_t height);
    void SetupDComp(HWND hwnd);
    void CreateSyncFence();
    void CreateHitTestStagingTextures();
    void UpdateHitTestCache();
    bool IsCursorOverVisiblePixel() const;
    void SetWindowClickThrough(bool enable);

    HWND m_hwnd = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
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
    HANDLE m_sharedFenceHandle = nullptr;
    HANDLE m_sharedTextureHandle = nullptr;

    static constexpr int HitTestSampleSize = 16;
    static constexpr uint8_t HitTestAlphaThreshold = 16;
    std::array<HitTestReadback, 3> m_hitTestReadbacks;
    uint32_t m_hitTestReadbackIndex = 0;
    std::array<uint8_t, HitTestSampleSize * HitTestSampleSize> m_hitTestAlphaCache{};
    bool m_hasHitTestCache = false;
    int m_hitTestCacheX = 0;
    int m_hitTestCacheY = 0;
    int m_hitTestCacheWidth = 0;
    int m_hitTestCacheHeight = 0;
    bool m_windowClickThroughEnabled = false;
};
} // namespace WindowSystemHost
