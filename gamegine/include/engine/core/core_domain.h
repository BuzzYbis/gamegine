// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 BuzzY_ & Rether
//
// core_domain.h                                                      -*-C++-*-
#ifndef INCLUDED_ENGINE_CORE_DOMAIN_H
#define INCLUDED_ENGINE_CORE_DOMAIN_H

//@PURPOSE: Provide the subsystems that raise errors and log messages.
//
//@CLASSES:
//  engine::core::Domain: subsystem an error or a log message comes from
//
//@SEE_ALSO: core_error, core_error_catalog, core_log_message,
//           core_log_message_catalog
//
//@DESCRIPTION: This component provides [Domain], the 8-bit enumeration of
// the subsystems an error or a log message can come from, and
// [k_DOMAIN_NAMES], the name of each. They live on their own so that errors
// and log messages share one list that cannot drift apart; [Error::Domain]
// is an alias of [Domain].
//
///Usage
///-----
// This section illustrates intended use of this component.
//
///Example 1: Branching on the Domain
/// - - - - - - - - - - - - - - - - -
// Pick a recovery from the subsystem that failed:
// ```
//  if (error.domain() == engine::core::Domain::e_VULKAN) {
//      recreate_device();
//  }
// ```

// std
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace engine::core {

/// Enumeration used to identify the subsystem raising an error or logging a
/// message.
enum class Domain : std::uint8_t {
    e_UNKNOWN = 0,  // no subsystem claims it
    e_CORE,         // the engine core
    e_VULKAN,       // the Vulkan backend
    e_METAL,        // the Metal backend

    e_COUNT,  // number of domains: always last, never raised
};

/// Name of each [Domain], in enumerator order.
inline constexpr auto k_DOMAIN_NAMES = std::to_array<std::string_view>({
    "unknown",
    "core",
    "vulkan",
    "metal",
});

// One name per enumerator: a new domain without its name does not compile.
static_assert(k_DOMAIN_NAMES.size() == std::to_underlying(Domain::e_COUNT));

}  // close namespace engine::core

#endif
