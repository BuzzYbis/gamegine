// EXPECT-ERROR: deleted
// One handle, one thread, one ring: a copy would be a second owner of it.
#include "engine/core/core_log.h"

using engine::core::Result;
using engine::core::error::Error;
using engine::core::log::Logger;
using engine::core::log::Severity;

void use(Logger& logger)
{
    Result<Logger::Handle> handle = logger.register_current_thread("t");
    if (!handle) {
        return;
    }
#ifdef GOOD
    Logger::Handle& same = *handle;
    (void)same;
#else
    Logger::Handle copy = *handle;
    (void)copy;
#endif
}
