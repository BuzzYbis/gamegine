// rhi_factory.cpp                                                    -*-C++-*-
#include <rhi/rhi_factory.h>

// std
#include <iostream>
#include <stdexcept>

// rhi
#include <rhi/rhi_contextprotocol.h>

// vlk
#include <rhi/vlk/vlk_context.h>

namespace eng::rhi {

std::unique_ptr<ContextProtocol> Factory::createContext(const GraphicsAPI api,
                                                        core::Window* window)
{
    switch (api) {
    case GraphicsAPI::Vulkan:
        std::cout << "[RHIFactory] Initializing Vulkan Backend..."
                  << std::endl;
        return std::make_unique<vlk::Context>(window);

    default:
        throw std::runtime_error(
            "[RHIFactory] Fatal Error: Unknown Graphics API requested!");
    }
}

}  // close package namespace