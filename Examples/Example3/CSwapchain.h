/*
*  This unit wraps the swapchain.
*
*  WARNING: This unit is a work in progress.
*  Interfaces are highly experimental and very likely to change.
*/

#ifndef CSWAPCHAIN_H
#define CSWAPCHAIN_H

#include "CDevices.h"

#define IS_ANDROID false // PC: default to low-latency (no fps limit
#ifdef ANDROID
#define IS_ANDROID true  // ANDROID: default to power-save (limit to 60fps)
#endif

struct CSwapchainBuffer {
  VkImage       image;
  VkImageView   view;
  VkFramebuffer frameBuffer;
};

class CSwapchain {
    VkPhysicalDevice   gpu;
    VkDevice           device;
    VkQueue            queue;
    VkSurfaceKHR       surface;
    VkSwapchainKHR     swapchain;
    VkRenderPass       renderpass;

    //VkSurfaceFormatKHR format;
    //VkPresentModeKHR   mode;
    //VkExtent2D         extent;

    std::vector<CSwapchainBuffer> buffers;
    uint32_t acquired_index;  // index of last acquired image

    void Init(VkPhysicalDevice gpu, VkDevice device, VkSurfaceKHR surface);

  public:
    VkSurfaceCapabilitiesKHR surface_caps;
    VkSwapchainCreateInfoKHR swapchain_info;

    //CSwapchain(VkPhysicalDevice gpu, VkDevice device, VkSurfaceKHR surface);
    CSwapchain(const CQueue& present_queue);
    ~CSwapchain();

    bool PresentMode(bool no_tearing, bool powersave = IS_ANDROID);  // ANDROID: default to power-save mode (limit to 60fps)
    bool PresentMode(VkPresentModeKHR preferred_mode);               // If mode is not available, returns false and uses FIFO.

    void SetFormat(VkFormat preferred_format = VK_FORMAT_B8G8R8A8_UNORM);
    void SetExtent(uint32_t width, uint32_t height);
    bool SetImageCount(uint32_t image_count = 2);                    // 2=doublebuffer 3=tripplebuffer
    void SetRenderPass(VkRenderPass renderpass);

    void Apply();
    void Print();    

    //uint32_t AcquireNextImage(VkSemaphore present_semaphore);
    //void Present(VkQueue queue, uint32_t index);

    //void Present(VkSemaphore present_semaphore, VkQueue queue);

    CSwapchainBuffer& AcquireNextImage(VkSemaphore present_semaphore = 0);
    void Present();
};

#endif


