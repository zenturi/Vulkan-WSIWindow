#ifndef CTRIANGLE_H
#define CTRIANGLE_H

#include "WSIWindow.h"
#include "CSwapchain.h"

class CTriangle {
    VkPipelineLayout pipelineLayout;
    VkPipeline       graphicsPipeline;
    VkShaderModule   vertShaderModule;
    VkShaderModule   fragShaderModule;

    static std::vector<char> ReadFile(const std::string& filename);
    void CreateShaderModule(const std::vector<char>& code, VkShaderModule& shaderModule);
  public:
    VkDevice     device;
    VkRenderPass renderpass;

    CTriangle();
    ~CTriangle();
    void CreateGraphicsPipeline(VkExtent2D extent = {64,64});
    void RecordCommandBuffer(CSwapchainBuffer& swapchain_buffer);
};

#endif

