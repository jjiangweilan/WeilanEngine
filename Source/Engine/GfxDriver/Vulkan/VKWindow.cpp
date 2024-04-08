#include "VKWindow.hpp"
#include "VKSwapchainImage.hpp"
#include <SDL_vulkan.h>
#include <spdlog/spdlog.h>

namespace Gfx
{
VKWindow::VKWindow(SDL_Window* window, int swapchainCount) : swapchainCount(swapchainCount)
{
    VKContext* c = VKContext::Instance();
    if (!SDL_Vulkan_CreateSurface(window, c->instance, &surface.handle))
    {
        spdlog::critical("Window surface creation failed: {0}", SDL_GetError());
    }

    surface.QuerySurfaceProperties(c->gpu->handle);
    swapchain.CreateOrOverrideSwapChain(surface, this->swapchainCount);

    VkSemaphoreCreateInfo semaphoreCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, VK_NULL_HANDLE, 0};
    c->objManager->CreateSemaphore(semaphoreCreateInfo, imageAcquireSemaphore);
    c->objManager->CreateSemaphore(semaphoreCreateInfo, presentSemaphore);

    // VKWindow is created between VKDriver::BeginFrame and VKDriver::EndFrame, we need to request the image for EndFrame
    VkResult acquireResult = vkAcquireNextImageKHR(
        c->device,
        swapchain.handle,
        -1,
        imageAcquireSemaphore,
        VK_NULL_HANDLE,
        &swapchainIndex
    );
    swapchain.swapchainImage->SetActiveSwapChainImage(swapchainIndex);
}

VKWindow::~VKWindow()
{
    VKContext* c = VKContext::Instance();
    c->objManager->DestroySemaphore(imageAcquireSemaphore);
    c->objManager->DestroySemaphore(presentSemaphore);

    vkDestroySwapchainKHR(c->device, swapchain.handle, VK_NULL_HANDLE);
    vkDestroySurfaceKHR(c->instance, surface.handle, VK_NULL_HANDLE);
}

Image* VKWindow::GetSwapchainImage()
{
    return swapchain.swapchainImage.get();
}

void VKWindow::SetSurfaceSize(int width, int height)
{
    recreateRequest.requested = false;
    recreateRequest.width = width;
    recreateRequest.height = height;
}
} // namespace Gfx
