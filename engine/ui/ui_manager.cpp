// engine.cpp                                                         -*-C++-*-
#include <ui/ui_manager.h>

namespace engine::ui {
void UIManager::initialize(const rhi::vulkan::VulkanContext&   context,
                           const rhi::vulkan::VulkanSwapchain& swapchain,
                           const core::Window&                 window)
{
    d_context = UIContext();
    d_context.initialize(context, swapchain, window);
}

void UIManager::beginFrame()
{
    d_context.beginFrame();
}

void UIManager::cleanUpFrame()
{
    d_context.cleanUpFrame();
}

void UIManager::renderPanels()
{
    for (const auto& panel : d_panels) {
        panel->render();
    }
}

void UIManager::endFrame(vk::CommandBuffer cmd)
{
    d_context.endFrame(cmd);
}

void UIManager::shutdown()
{
    d_context.shutdown();
}

void UIManager::addPanel(std::unique_ptr<UIPanel> panel)
{
    d_panels.push_back(std::move(panel));
}

}  // close engine::ui namespace