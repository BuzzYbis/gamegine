// EXPECT-ERROR: private
// Only 'Logger::register_current_thread' makes a handle: the producer it
// wraps is private to the logger, so no one else can even name it.
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
    Logger::Handle handle(*static_cast<Logger::Producer*>(nullptr));
    (void)logger;
    (void)handle;
#endif
}
