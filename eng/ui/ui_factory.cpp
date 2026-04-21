// ui_factory.cpp                                                     -*-C++-*-
#include <ui/ui_factory.h>

// std
#include <iostream>
#include <stdexcept>

// ui
#include <ui/ui_vulkanbackend.h>

namespace eng::ui {

// -------------
// class Factory
// -------------

// CLASS METHODS
std::unique_ptr<BackendProtocol> Factory::createContext(const rhi::GraphicsAPI api)
{
    switch (api) {
    case rhi::GraphicsAPI::Vulkan:
        std::cout << "[RHIFactory] Initializing Vulkan Backend..."
                  << std::endl;
        return std::make_unique<VulkanBackend>();


    default:
        throw std::runtime_error(
            "[RHIFactory] Fatal Error: Unknown Graphics API requested!");
    }
}

}  // close package namespace