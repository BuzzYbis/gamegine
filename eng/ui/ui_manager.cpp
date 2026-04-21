// engine.cpp                                                         -*-C++-*-
#include <ui/ui_manager.h>

// ui
#include <ui/ui_factory.h>

namespace eng::ui {

// ---------------
// class UIManager
// ---------------

// MANIPULATORS
void UIManager::initialize(rhi::ContextProtocol*   context,
                           rhi::SwapchainProtocol* swapchain,
                           const core::Window&     window,
                           rhi::GraphicsAPI        api)
{
    d_context = UIContext();
    d_backend = Factory::createContext(api);
    d_context.initialize(context, swapchain, window, d_backend);
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

void UIManager::endFrame(rhi::CommandListProtocol* cmd)
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

}  // close package namespace