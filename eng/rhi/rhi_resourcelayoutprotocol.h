// rhi_resourcelayoutprotocol.h                                       -*-C++-*-
#ifndef INCLUDED_ENG_RHI_RESOURCELAYOUTPROTOCOL_H
#define INCLUDED_ENG_RHI_RESOURCELAYOUTPROTOCOL_H

//@PURPOSE: Provide a pure abstract interface for GPU resource layouts.
//
//@CLASSES:
//  eng::rhi::ResourceLayoutProtocol: Protocol for descriptor set layouts.
//
//@DESCRIPTION: This component provides a pure abstract interface,
// 'eng::rhi::ResourceLayoutProtocol', that defines the schema of resources
// (UBOs, samplers) used by a shader. It acts as a template for allocating
// resource sets.

namespace eng::rhi {

// ============================
// class ResourceLayoutProtocol
// ============================

/// This class provides a protocol (pure abstract interface) representing the
/// layout of resources bound to a graphics pipeline.
class ResourceLayoutProtocol {
  public:
    // CREATORS

    /// Destroy this resource layout.
    virtual ~ResourceLayoutProtocol() = default;
};

}  // close package namespace
#endif  // INCLUDED_ENG_RHI_RESOURCELAYOUTPROTOCOL_H