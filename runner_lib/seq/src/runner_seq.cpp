#include "runner.h"


namespace runner {

    /// Sequential variant - used when the assignment forbids any parallel
    /// framework (allowedFrameworks empty / detected framework "none").
    /// No environment, no monitor, no thread-pool init. The student is
    /// expected to write straight-line code (or use std::thread directly).
    void setup() {
        // Intentionally empty.
    }

} // namespace runner
