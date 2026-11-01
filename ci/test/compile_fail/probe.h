// A message enumeration with one message for each argument there is, for the
// compile-fail snippets.
#pragma once

#include "engine/core/core_log_message.h"

#include <array>
#include <cstdint>

enum class Probe : std::uint16_t {
    e_NONE,
    e_UNSIGNED,
    e_SIGNED,
    e_HEX,
    e_FLOAT,

    e_COUNT,
};

template <> struct engine::core::log::LogMessageIdTraits<Probe> {
    static constexpr Domain k_DOMAIN = Domain::e_CORE;

    static constexpr auto k_ARGUMENTS =
        std::to_array<LogMessage::Argument>({
            LogMessage::Argument::e_NONE,
            LogMessage::Argument::e_UNSIGNED,
            LogMessage::Argument::e_SIGNED,
            LogMessage::Argument::e_HEX,
            LogMessage::Argument::e_FLOAT,
        });
};
