// EXPECT-ERROR: no matching
// A handle is a thread's right to log: it cannot be made out of nothing.
#include "engine/core/core_log.h"

using engine::core::Result;
using engine::core::error::Error;
using engine::core::log::Logger;
using engine::core::log::Severity;

void use(Logger& logger)
{
#ifdef GOOD
    Result<Logger::Handle> handle = logger.register_current_thread("t");
    (void)handle;
#else
    Logger::Handle handle;
    (void)logger;
    (void)handle;
#endif
}
