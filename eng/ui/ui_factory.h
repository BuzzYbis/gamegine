// ui_factory.h                                                       -*-C++-*-
#ifndef INCLUDED_ENG_UI_UI_FACTORY_H
#define INCLUDED_ENG_UI_UI_FACTORY_H

//@PURPOSE: Provide a factory for creating UI backend protocol implementations.
//
//@CLASSES:
//  eng::ui::Factory: Utility class for instantiating UI backends.
//
//@DESCRIPTION: This component provides a utility class, 'eng::ui::Factory',
// that serves as the entry point for creating the appropriate UI backend
// (e.g., Vulkan) based on the requested graphics API. It abstracts the
// concrete backend types behind the 'BackendProtocol' interface.

// std
#include <memory>

// rhi
#include <rhi/rhi_types.h>

// ui
#include <ui/ui_backendprotocol.h>

namespace eng::ui {

// =============
// class Factory
// =============

/// Utility class for instantiating UI backend protocol implementations.
class Factory {
  public:
    // CLASS METHODS

    /// Create and return a newly allocated backend implementation of the
    /// 'BackendProtocol' corresponding to the specified 'api'. Throw an
    /// exception if the specified 'api' is not supported.
    static std::unique_ptr<BackendProtocol>
    createContext(rhi::GraphicsAPI api);
};

}  // close package namespace

#endif  // INCLUDED_ENG_UI_UI_FACTORY_H
