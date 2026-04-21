// rhi_factory.h                                                      -*-C++-*-
#ifndef INCLUDED_ENG_RHI_FACTORY_H
#define INCLUDED_ENG_RHI_FACTORY_H

//@PURPOSE: Provide a factory for creating Render Hardware Interface objects.
//
//@CLASSES:
//  eng::rhi::Factory: Utility class for RHI context instantiation.
//
//@DESCRIPTION: This component provides a utility class, 'eng::rhi::Factory',
// that serves as the main entry point for instantiating the appropriate
// graphics API backend (e.g., Vulkan). It abstracts the creation process,
// returning a platform-agnostic 'ContextProtocol' that the rest of the engine
// can use without knowing the underlying implementation details.

// std
#include <memory>

// core
#include <core/core_window.h>

// rhi
#include <rhi/rhi_contextprotocol.h>
#include <rhi/rhi_types.h>

namespace eng::rhi {

// =============
// class Factory
// =============

/// This utility class provides a factory method to instantiate the graphics
/// context based on the selected rendering API.
class Factory {
  public:
    // CLASS METHODS

    /// Create and return a unique pointer to a new graphics context
    /// implementing the 'ContextProtocol' corresponding to the specified
    /// 'api', and bind it to the specified 'window'. Return an empty pointer
    /// if the creation fails or if the requested API is not supported. The
    /// behavior is undefined unless 'window' is non-null and remains valid for
    /// the lifetime of the returned context.
    static std::unique_ptr<ContextProtocol>
    createContext(GraphicsAPI api, core::Window* window);
};

}  // close package namespace
#endif  // INCLUDED_ENG_RHI_FACTORY_H