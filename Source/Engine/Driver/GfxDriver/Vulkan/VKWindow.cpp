#include "VKWindow.hpp"
#include "VKSwapchainImage.hpp"
#include <SDL_vulkan.h>
#include <spdlog/spdlog.h>

namespace Gfx
{
VKWindow::VKWindow(SDL_Window* window, int swapchainCount) : window(window), swapchainCount(swapchainCount)
{
    VKContext* c = VKContext::Instance();
    swapchain.handle = VK_NULL_HANDLE;
    surface.handle = VK_NULL_HANDLE;

    if (!SDL_Vulkan_CreateSurface(window, c->instance, &surface.handle))
    {
        spdlog::critical("Window surface creation failed: {0}", SDL_GetError());
    }

    SetSurfaceSizeImple(0, 0);
}

VKWindow::~VKWindow()
{
    VKContext* c = VKContext::Instance();
    for (auto s : imageAcquireSemaphores)
    {
        c->objManager->DestroySemaphore(s);
    }
    for (auto s : presentSemaphores)
    {
        c->objManager->DestroySemaphore(s);
    }

    vkDestroySwapchainKHR(c->device, swapchain.handle, VK_NULL_HANDLE);
    vkDestroySurfaceKHR(c->instance, surface.handle, VK_NULL_HANDLE);
}

Image* VKWindow::GetSwapchainImage()
{
    return swapchain.swapchainImage.get();
}

void VKWindow::SetSurfaceSize(int width, int height) {}

void VKWindow::SetSurfaceSizeImple(int width, int height)
{

    VKContext* c = VKContext::Instance();

    surface.QuerySurfaceProperties(c->gpu->handle);

    swapchain.CreateOrOverrideSwapChain(surface, this->swapchainCount, 0, 0);

    VkSemaphoreCreateInfo semaphoreCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, VK_NULL_HANDLE, 0};

    for (auto s : imageAcquireSemaphores)
    {
        c->objManager->DestroySemaphore(s);
    }

    for (auto s : presentSemaphores)
    {
        c->objManager->DestroySemaphore(s);
    }
    imageAcquireSemaphores.clear();
    presentSemaphores.clear();

    for (int i = 0; i < this->swapchainCount; ++i)
    {
        VkSemaphore imageAcquireSemaphore;
        VkSemaphore presentSemaphore;
        c->objManager->CreateSemaphore(semaphoreCreateInfo, imageAcquireSemaphore);
        c->objManager->CreateSemaphore(semaphoreCreateInfo, presentSemaphore);
        imageAcquireSemaphores.push_back(imageAcquireSemaphore);
        presentSemaphores.push_back(presentSemaphore);
    }

    // VKWindow is created between VKDriver::BeginFrame and VKDriver::EndFrame, we need to request the image for
    // EndFrame
    activeIndex = 0;
    vkAcquireNextImageKHR(
        c->device,
        swapchain.handle,
        -1,
        imageAcquireSemaphores[activeIndex],
        VK_NULL_HANDLE,
        &swapchainIndex
    );
    swapchain.swapchainImage->SetActiveSwapChainImage(swapchainIndex);
}
} // namespace Gfx
