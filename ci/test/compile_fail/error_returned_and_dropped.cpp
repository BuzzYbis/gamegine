// EXPECT-ERROR: discarded
// An error comes back from a function and is dropped. The class carries its
// own [[nodiscard]], apart from the one on Error::make.
#include "engine/core/core_error.h"

using engine::core::error::Error;

[[maybe_unused]] Error produce();

#ifdef GOOD
[[maybe_unused]] void f() { const Error kept = produce(); (void)kept; }
#else
[[maybe_unused]] void f() { produce(); }
#endif
