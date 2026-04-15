#pragma once

#include <functional>

/// RAII guard: runs `fn` (if set) at scope exit. Used to defer cleanup such
/// as temp-dir removal, mutex release, or DLL unloading without juggling
/// goto/try/catch.
struct ScopeCleanup {
    std::function<void()> fn;
    ~ScopeCleanup() { if(fn) fn(); }
    ScopeCleanup(const ScopeCleanup&) = delete;
    ScopeCleanup& operator=(const ScopeCleanup&) = delete;
    ScopeCleanup(std::function<void()> f) : fn(std::move(f)) {}
};
