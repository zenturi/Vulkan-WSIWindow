#include "CSwapchain.h"

int clamp(int val, int min, int max){ return (val < min ? min : val > max ? max : val); }

//CSwapchain::CSwapchain(VkPhysicalDevice gpu, VkDevice device, VkSurfaceKHR surface){
//    Init(gpu, device, surface);
//}

CSwapchain::CSwapchain(const CQueue& present_queue){
    const CQueue& q = present_queue;
    if(!q.surface){ LOGE("This queue may not be presentable. (No surface attached.)"); }
    Init(q.gpu, q.device, q.surface);
    queue = q.handle;
}

CSwapchain::~CSwapchain(){
    if (swapchain) {
        vkDeviceWaitIdle(device);
        for(auto& buf : buffers)  vkDestroyImageView(device, buf.view, nullptr);
        vkDestroySwapchainKHR(device, swapchain, 0);
        LOGI("Swapchain destroyed\n");
    }
}

void CSwapchain::Init(VkPhysicalDevice gpu, VkDevice device, VkSurfaceKHR surface){
    this->gpu     = gpu;
    this->surface = surface;
    this->device  = device;
    swapchain     = 0;
    renderpass    = 0;

    //--- surface caps ---
    VKERRCHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &surface_caps));
    assert(surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    assert(surface_caps.supportedTransforms & surface_caps.currentTransform);
    assert(surface_caps.supportedCompositeAlpha & (VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR | VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR));
    //--------------------

    swapchain_info = {};
    swapchain_info.sType                 = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_info.surface               = surface;
    //swapchain_info.minImageCount         = 2; // double-buffer
    //swapchain_info.imageFormat           = format.format;
    //swapchain_info.imageColorSpace       = format.colorSpace;
    //swapchain_info.imageExtent           = {64, 64}; //extent;
    swapchain_info.imageArrayLayers      = 1;  // 2 for stereo
    swapchain_info.imageUsage            = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_info.preTransform          = surface_caps.currentTransform;  //VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ?
    //swapchain_info.compositeAlpha        = composite_alpha;
    swapchain_info.presentMode           = VK_PRESENT_MODE_FIFO_KHR;
    swapchain_info.clipped               = true;
    //swapchain_info.oldSwapchain          = swapchain;
    swapchain_info.compositeAlpha = (surface_caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) ?
                                    VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR : VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    SetExtent(64, 64);  // also initializes surface_caps
    SetFormat(VK_FORMAT_B8G8R8A8_UNORM);
    SetImageCount(2);
    //Apply();
}

void CSwapchain::SetExtent(uint32_t width, uint32_t height){
    VKERRCHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &surface_caps));
    VkExtent2D& curr = surface_caps.currentExtent;
    VkExtent2D& ext = swapchain_info.imageExtent;

    //printf("swapchain: w=%d h=%d curr_w=%d curr_h=%d\n", width, height, curr.width, curr.height);
    if (curr.width == 0xFFFFFFFF == curr.height){
        ext.width  = clamp(width,  surface_caps.minImageExtent.width,  surface_caps.maxImageExtent.width);
        ext.height = clamp(height, surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height);
    } else ext = curr;
    if (!!swapchain) Apply();
}

void CSwapchain::SetFormat(VkFormat preferred_format){  // if preferred is not available, default to first available format
    //---Get Surface format list---
    std::vector<VkSurfaceFormatKHR> formats;
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, nullptr);
    formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, formats.data());
    //-----------------------------

    VkFormat& format = swapchain_info.imageFormat;
    for (auto& f : formats) if (!format || f.format == preferred_format) format = f.format;
    if (!format) format = preferred_format;
    if (format != preferred_format) LOGW("Surface format %d not available. Using format %d instead.\n", preferred_format, format);
    if (!!swapchain) Apply();
}

bool CSwapchain::SetImageCount(uint32_t image_count){  // set number of framebuffers. (2 or 3)
    uint32_t count = max(image_count, surface_caps.minImageCount);                      //clamp to min limit
    if(surface_caps.maxImageCount > 0) count = min(count, surface_caps.maxImageCount);  //clamp to max limit
    swapchain_info.minImageCount = count;
    return (count == image_count);
    //CreateFramebuffers(image_count);
    //CreateBackBuffers(image_count);
    if(!!swapchain) Apply();
}

// Returns the list of avialable present modes for this gpu + surface.
std::vector<VkPresentModeKHR> GetPresentModes(VkPhysicalDevice gpu, VkSurfaceKHR surface){
    uint32_t count = 0;
    std::vector<VkPresentModeKHR> modes;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, nullptr);
    assert(count > 0);
    modes.resize(count);
    VKERRCHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &count, modes.data()));
    return modes;
}

// ---------------------------- Present Mode ----------------------------
// no_tearing : TRUE = Wait for next vsync, to swap buffers.  FALSE = faster fps.
// powersave  : TRUE = Limit framerate to vsync (60 fps).     FALSE = lower latency.
bool CSwapchain::PresentMode(bool no_tearing, bool powersave){
    return PresentMode(VkPresentModeKHR ((no_tearing ? 1 : 0) ^ (powersave ? 3 : 0)));  // if not found, use FIFO
}

bool CSwapchain::PresentMode(VkPresentModeKHR pref_mode){
    VkPresentModeKHR& mode = swapchain_info.presentMode;
    auto modes = GetPresentModes(gpu, surface);
    mode = VK_PRESENT_MODE_FIFO_KHR;                           // default to FIFO mode
    for (auto m : modes) if(m == pref_mode) mode = pref_mode;  // if prefered mode is available, select it.
    if (mode != pref_mode) LOGW("Requested present-mode is not supported. Reverting to FIFO mode.\n");
    if (!!swapchain) Apply();
    return (mode == pref_mode);
}
//-----------------------------------------------------------------------


const char* FormatStr(VkFormat fmt) {
#define STR(f) case f: return #f
    switch (fmt) {
        STR(VK_FORMAT_R5G6B5_UNORM_PACK16);  // 4
        STR(VK_FORMAT_R8G8B8A8_UNORM);       // 37
        STR(VK_FORMAT_R8G8B8A8_SRGB);        // 43
        STR(VK_FORMAT_B8G8R8A8_UNORM);       // 44
        STR(VK_FORMAT_B8G8R8A8_SRGB);        // 50
        default: return "";
    }
#undef STR
}

void CSwapchain::Print(){
    printf("Swapchain:\n");
    printf("\tFormat  = %d : %s\n", swapchain_info.imageFormat, FormatStr(swapchain_info.imageFormat));
    VkExtent2D& extent = swapchain_info.imageExtent;
    printf("\tExtent  = %d x %d\n", extent.width, extent.height);
    printf("\tBuffers = %d\n", (int)buffers.size());

    auto modes = GetPresentModes(gpu, surface);
    printf("\tPresentMode:\n");
    const char* mode_names[] = {"VK_PRESENT_MODE_IMMEDIATE_KHR", "VK_PRESENT_MODE_MAILBOX_KHR",
                                "VK_PRESENT_MODE_FIFO_KHR", "VK_PRESENT_MODE_FIFO_RELAXED_KHR"};
    VkPresentModeKHR& mode = swapchain_info.presentMode;
    for (auto m : modes) print((m == mode) ? eRESET : eFAINT, "\t\t%s %s\n", (m == mode) ? cTICK : " ",mode_names[m]);
}

void CSwapchain::SetRenderPass(VkRenderPass renderpass){
    this->renderpass = renderpass;
    Apply();
}

void CSwapchain::Apply(){
    assert(!!renderpass && "RendePass was not set.");

    vkDeviceWaitIdle(device);
    swapchain_info.oldSwapchain = swapchain;
    VKERRCHECK(vkCreateSwapchainKHR(device, &swapchain_info, nullptr, &swapchain));
    vkDestroySwapchainKHR(device, swapchain_info.oldSwapchain, nullptr);

    std::vector<VkImage> images;
    uint32_t count = 0;
    VKERRCHECK(vkGetSwapchainImagesKHR(device, swapchain, &count, nullptr));
    images.resize(count);
    VKERRCHECK(vkGetSwapchainImagesKHR(device, swapchain, &count, images.data()));

    for(auto& buf : buffers) vkDestroyImageView(device, buf.view, nullptr);  // Delete old ImageViews

    buffers.resize(count);
    repeat(count){
        auto& buf = buffers[i];
        buf.image = images[i];
        //---ImageView---
        VkImageViewCreateInfo ivCreateInfo = {};
        ivCreateInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        ivCreateInfo.pNext    = NULL;
        ivCreateInfo.flags    = 0;
        ivCreateInfo.image    = images[i];
        ivCreateInfo.format   = swapchain_info.imageFormat;
        ivCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ivCreateInfo.components = {};
        ivCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        ivCreateInfo.subresourceRange.baseMipLevel   = 0;
        ivCreateInfo.subresourceRange.levelCount     = 1;
        ivCreateInfo.subresourceRange.baseArrayLayer = 0;
        ivCreateInfo.subresourceRange.layerCount     = 1;
        VKERRCHECK(vkCreateImageView(device, &ivCreateInfo, nullptr, &buf.view));
        //---------------
        //--Framebuffer--
        VkFramebufferCreateInfo fbCreateInfo = {};
        fbCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbCreateInfo.attachmentCount = 1;
        fbCreateInfo.pAttachments = &buf.view;
        fbCreateInfo.width  = swapchain_info.imageExtent.width;
        fbCreateInfo.height = swapchain_info.imageExtent.height;
        fbCreateInfo.layers = 1;
        fbCreateInfo.renderPass = renderpass;
        VKERRCHECK(vkCreateFramebuffer(device, &fbCreateInfo, NULL, &buf.frameBuffer));
        //---------------
    }
    if (!swapchain_info.oldSwapchain) LOGI("Swapchain created\n");
}
/*
uint32_t CSwapchain::AcquireNextImage(VkSemaphore present_semaphore){
    uint32_t index;
    VKERRCHECK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, present_semaphore, (VkFence)0, &index));
    return index;
}

void CSwapchain::Present(VkQueue queue, uint32_t index) {
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = NULL;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &index;
    VKERRCHECK(vkQueuePresentKHR(queue, &presentInfo));
}
*/
/*
void CSwapchain::Present(VkSemaphore present_semaphore, VkQueue queue) {
    uint32_t index;
    VKERRCHECK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, present_semaphore, (VkFence)0, &index));

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = NULL;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &index;
    VKERRCHECK(vkQueuePresentKHR(queue, &presentInfo));
}
*/
CSwapchainBuffer& CSwapchain::AcquireNextImage(VkSemaphore present_semaphore){
    assert(!!swapchain && !!renderpass);
    VKERRCHECK(vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, present_semaphore, (VkFence)0, &acquired_index));
    return buffers[acquired_index];
}

void CSwapchain::Present() {
    assert(!!swapchain && !!renderpass);
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.pNext = NULL;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &acquired_index;
    VKERRCHECK(vkQueuePresentKHR(queue, &presentInfo));
}

