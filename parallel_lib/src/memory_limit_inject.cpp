/**
 * @file memory_limit_inject.cpp
 * @brief Comprehensive allocator interception for student DLL memory tracking.
 *
 * This file is NOT compiled into parallel_lib. Instead, cmake_generator injects
 * it into student_solution.dll builds via target_sources() + --wrap linker flags.
 *
 * Intercepts:
 *   C core:         malloc, free, calloc, realloc                    (--wrap)
 *   C strings:      strdup, _strdup, wcsdup, _wcsdup                 (--wrap)
 *   POSIX strings:  strndup                                          (--wrap, Linux)
 *   POSIX misc:     reallocarray, asprintf, vasprintf                (--wrap, Linux)
 *   Aligned (Linux): aligned_alloc, posix_memalign, memalign,
 *                     valloc, pvalloc                                 (--wrap)
 *   Aligned (Win):   _aligned_malloc, _aligned_free, _aligned_realloc (--wrap)
 *   VM (Linux):      mmap, munmap, sbrk                              (--wrap)
 *   C++ regular:     operator new/delete (all variants)               (source override)
 *   C++ aligned:     operator new/delete with align_val_t (C++17)     (source override)
 *
 * Layout — unified Meta header for heap allocations:
 *
 *   Regular:  [Meta: magic, raw=start, total=N] [user data...]
 *              ^raw_ptr                          ^returned
 *
 *   Aligned:  [padding...] [Meta: magic, raw=start, total=N] [user data (aligned)...]
 *              ^raw_ptr                                        ^returned
 *
 * Foreign pointer safety: Meta contains a 64-bit magic number. On free/realloc,
 * the magic is checked first. If it doesn't match (pointer from libc's internal
 * malloc, e.g. getline/realpath), we pass through to __real_free/__real_realloc.
 * This prevents crashes when student code frees memory allocated by C library
 * functions that bypass --wrap.
 *
 * VM allocations (mmap) use a lock-free side table instead of Meta headers.
 *
 * Thread safety: par::memory::detail functions use atomics + thread-local context.
 * C++ overrides call __real_malloc/__real_free to avoid double-counting.
 */

#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <new>
#include <par/memory_guard.h>

#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif

// ============================================================================
// Unified allocation header
// ============================================================================

static constexpr std::uint64_t META_MAGIC = 0xA110'CA7E'DEAD'CAFEull;

struct alignas(alignof(std::max_align_t)) Meta {
    std::uint64_t magic;    // META_MAGIC — identifies our allocations vs foreign
    void*         raw_ptr;  // original __real_malloc result (== &meta for regular, != for aligned)
    std::size_t   total;    // total bytes allocated (for tracking)
};
// sizeof(Meta) == 32 on 64-bit (24 bytes data, padded to 32 for alignment)
// sizeof(Meta) == 16 on 32-bit (16 bytes data, aligned to 8)

// ============================================================================
// Forward declarations for __real_* symbols provided by --wrap linker
// ============================================================================

extern "C" {
    void* __real_malloc(std::size_t size);
    void  __real_free(void* ptr);
    void* __real_realloc(void* ptr, std::size_t size);

#ifndef _WIN32
    void* __real_mmap(void* addr, std::size_t length, int prot, int flags,
                      int fd, off_t offset);
    int   __real_munmap(void* addr, std::size_t length);
    void* __real_sbrk(intptr_t increment);
#endif
}

// ============================================================================
// Helpers
// ============================================================================

// Check if a pointer has our Meta header
static inline bool is_our_allocation(void* p) {
    auto* meta = reinterpret_cast<Meta*>(static_cast<char*>(p) - sizeof(Meta));
    return meta->magic == META_MAGIC;
}

// Read Meta from a tracked pointer (caller must verify is_our_allocation first)
static inline Meta* get_meta(void* p) {
    return reinterpret_cast<Meta*>(static_cast<char*>(p) - sizeof(Meta));
}

// Stamp Meta fields on a new allocation
static inline void stamp_meta(Meta* meta, void* raw_ptr, std::size_t total) {
    meta->magic   = META_MAGIC;
    meta->raw_ptr = raw_ptr;
    meta->total   = total;
}

// ============================================================================
// Helper: aligned allocation using over-allocate + align strategy
// (for C allocator wrappers — uses tryAlloc for non-throwing behavior)
// ============================================================================

static void* aligned_alloc_impl(std::size_t alignment, std::size_t size) {
    if (alignment < sizeof(Meta)) alignment = sizeof(Meta);

    std::size_t total = size + alignment + sizeof(Meta);

    if (!par::memory::detail::tryAlloc(static_cast<long long>(total)))
        return nullptr;

    void* raw = __real_malloc(total);
    if (!raw) {
        par::memory::detail::onFree(static_cast<long long>(total));
        return nullptr;
    }

    std::uintptr_t base    = reinterpret_cast<std::uintptr_t>(raw) + sizeof(Meta);
    std::uintptr_t aligned = (base + alignment - 1) & ~(alignment - 1);

    auto* meta = reinterpret_cast<Meta*>(aligned) - 1;
    stamp_meta(meta, raw, total);

    return reinterpret_cast<void*>(aligned);
}

// ============================================================================
// Core C allocator wrappers
// ============================================================================

extern "C" void* __wrap_malloc(std::size_t size) {
    std::size_t total = size + sizeof(Meta);

    if (!par::memory::detail::tryAlloc(static_cast<long long>(total)))
        return nullptr;

    void* raw = __real_malloc(total);
    if (!raw) {
        par::memory::detail::onFree(static_cast<long long>(total));
        return nullptr;
    }

    auto* meta = static_cast<Meta*>(raw);
    stamp_meta(meta, raw, total);
    return reinterpret_cast<char*>(raw) + sizeof(Meta);
}

extern "C" void __wrap_free(void* p) {
    if (!p) return;

    if (!is_our_allocation(p)) {
        // Foreign pointer (e.g. from libc's getline/realpath) — pass through
        __real_free(p);
        return;
    }

    auto* meta = get_meta(p);
    par::memory::detail::onFree(static_cast<long long>(meta->total));
    __real_free(meta->raw_ptr);
}

extern "C" void* __wrap_calloc(std::size_t count, std::size_t elem_size) {
    if (elem_size != 0 && count > SIZE_MAX / elem_size) return nullptr;  // overflow check
    std::size_t size = count * elem_size;
    void* p = __wrap_malloc(size);
    if (p) std::memset(p, 0, size);
    return p;
}

extern "C" void* __wrap_realloc(void* p, std::size_t new_size) {
    if (!p) return __wrap_malloc(new_size);
    if (new_size == 0) { __wrap_free(p); return nullptr; }

    if (!is_our_allocation(p)) {
        // Foreign pointer — pass through, result stays untracked
        return __real_realloc(p, new_size);
    }

    auto* old_meta = get_meta(p);
    bool was_aligned = (old_meta->raw_ptr != reinterpret_cast<void*>(old_meta));

    if (was_aligned) {
        // Cannot __real_realloc aligned memory — allocate new, copy, free old
        void* new_p = __wrap_malloc(new_size);
        if (!new_p) return nullptr;
        std::size_t old_data = old_meta->total - static_cast<std::size_t>(
            static_cast<char*>(p) - static_cast<char*>(old_meta->raw_ptr));
        std::memcpy(new_p, p, old_data < new_size ? old_data : new_size);
        __wrap_free(p);
        return new_p;
    }

    // Regular allocation — can use __real_realloc directly
    std::size_t new_total = new_size + sizeof(Meta);
    long long diff = static_cast<long long>(new_total) - static_cast<long long>(old_meta->total);

    if (diff > 0 && !par::memory::detail::tryAlloc(diff))
        return nullptr;

    void* new_raw = __real_realloc(old_meta->raw_ptr, new_total);
    if (!new_raw) {
        if (diff > 0) par::memory::detail::onFree(diff);
        return nullptr;
    }

    auto* new_meta = static_cast<Meta*>(new_raw);
    stamp_meta(new_meta, new_raw, new_total);

    if (diff < 0) par::memory::detail::onFree(-diff);
    return reinterpret_cast<char*>(new_raw) + sizeof(Meta);
}

// ============================================================================
// String duplication wrappers (both platforms)
// ============================================================================

extern "C" char* __wrap_strdup(const char* s) {
    if (!s) return nullptr;
    std::size_t len = std::strlen(s) + 1;
    void* p = __wrap_malloc(len);
    if (!p) return nullptr;
    std::memcpy(p, s, len);
    return static_cast<char*>(p);
}

extern "C" char* __wrap__strdup(const char* s) {
    return __wrap_strdup(s);
}

extern "C" wchar_t* __wrap_wcsdup(const wchar_t* s) {
    if (!s) return nullptr;
    std::size_t len = std::wcslen(s) + 1;
    std::size_t bytes = len * sizeof(wchar_t);
    void* p = __wrap_malloc(bytes);
    if (!p) return nullptr;
    std::memcpy(p, s, bytes);
    return static_cast<wchar_t*>(p);
}

extern "C" wchar_t* __wrap__wcsdup(const wchar_t* s) {
    return __wrap_wcsdup(s);
}

// ============================================================================
// Platform-specific allocators
// ============================================================================

#ifndef _WIN32

// --- POSIX string/array functions ---

extern "C" char* __wrap_strndup(const char* s, std::size_t n) {
    if (!s) return nullptr;
    std::size_t len = std::strnlen(s, n);
    char* p = static_cast<char*>(__wrap_malloc(len + 1));
    if (!p) return nullptr;
    std::memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

extern "C" void* __wrap_reallocarray(void* ptr, std::size_t count, std::size_t size) {
    if (count != 0 && size > SIZE_MAX / count) {
        errno = ENOMEM;
        return nullptr;
    }
    return __wrap_realloc(ptr, count * size);
}

// --- POSIX formatted string allocation ---

extern "C" int __wrap_vasprintf(char** strp, const char* fmt, std::va_list ap) {
    std::va_list ap_copy;
    va_copy(ap_copy, ap);
    int len = std::vsnprintf(nullptr, 0, fmt, ap_copy);
    va_end(ap_copy);

    if (len < 0) { *strp = nullptr; return -1; }

    *strp = static_cast<char*>(__wrap_malloc(static_cast<std::size_t>(len) + 1));
    if (!*strp) return -1;

    return std::vsnprintf(*strp, static_cast<std::size_t>(len) + 1, fmt, ap);
}

extern "C" int __wrap_asprintf(char** strp, const char* fmt, ...) {
    std::va_list ap;
    va_start(ap, fmt);
    int ret = __wrap_vasprintf(strp, fmt, ap);
    va_end(ap);
    return ret;
}

// --- Linux aligned allocators ---

extern "C" void* __wrap_aligned_alloc(std::size_t alignment, std::size_t size) {
    return aligned_alloc_impl(alignment, size);
}

extern "C" int __wrap_posix_memalign(void** memptr, std::size_t alignment, std::size_t size) {
    if (!memptr) return EINVAL;
    void* p = aligned_alloc_impl(alignment, size);
    if (!p) return ENOMEM;
    *memptr = p;
    return 0;
}

extern "C" void* __wrap_memalign(std::size_t alignment, std::size_t size) {
    return aligned_alloc_impl(alignment, size);
}

extern "C" void* __wrap_valloc(std::size_t size) {
    return aligned_alloc_impl(4096, size);
}

extern "C" void* __wrap_pvalloc(std::size_t size) {
    constexpr std::size_t page = 4096;
    std::size_t rounded = (size + page - 1) & ~(page - 1);
    return aligned_alloc_impl(page, rounded);
}

// ============================================================================
// mmap/munmap tracking via lock-free side table
//
// mmap allocations cannot use Meta headers (kernel returns page-aligned
// addresses, caller may depend on exact alignment). Instead, we track
// {address -> length} in a fixed-size table using atomic CAS.
//
// Only anonymous mappings (MAP_ANONYMOUS) are tracked — file-backed
// mappings are I/O, not memory allocation.
//
// Limitations:
//   - Table capacity 4096 entries (more than enough for student code)
//   - Partial munmap not tracked (only exact address match)
//   - Table full -> allocation proceeds untracked (under-count, no crash)
// ============================================================================

namespace {
    struct MmapEntry {
        std::atomic<std::uintptr_t> key{0};   // 0 = free, 1 = writing, >1 = address
        std::size_t                 length{0};
    };

    constexpr std::size_t       MMAP_TABLE_SIZE = 4096;
    constexpr std::uintptr_t    MMAP_SLOT_WRITING = 1;  // sentinel (mmap never returns 1)

    MmapEntry g_mmap_table[MMAP_TABLE_SIZE];

    void mmap_track(void* addr, std::size_t length) {
        auto tag = reinterpret_cast<std::uintptr_t>(addr);
        for (auto& e : g_mmap_table) {
            std::uintptr_t expected = 0;
            if (e.key.compare_exchange_strong(expected, MMAP_SLOT_WRITING,
                    std::memory_order_relaxed, std::memory_order_relaxed)) {
                e.length = length;
                e.key.store(tag, std::memory_order_release);
                return;
            }
        }
        // Table full — allocation proceeds untracked
    }

    std::size_t mmap_untrack(void* addr) {
        auto tag = reinterpret_cast<std::uintptr_t>(addr);
        for (auto& e : g_mmap_table) {
            if (e.key.load(std::memory_order_acquire) == tag) {
                std::size_t len = e.length;
                e.key.store(0, std::memory_order_release);
                return len;
            }
        }
        return 0;  // not found (file-backed mapping or table overflow)
    }
}

extern "C" void* __wrap_mmap(void* addr, std::size_t length, int prot, int flags,
                              int fd, off_t offset) {
    bool track = (flags & MAP_ANONYMOUS) && length > 0;

    if (track) {
        if (!par::memory::detail::tryAlloc(static_cast<long long>(length))) {
            errno = ENOMEM;
            return MAP_FAILED;
        }
    }

    void* result = __real_mmap(addr, length, prot, flags, fd, offset);

    if (result == MAP_FAILED) {
        if (track) par::memory::detail::onFree(static_cast<long long>(length));
        return MAP_FAILED;
    }

    if (track) mmap_track(result, length);
    return result;
}

extern "C" int __wrap_munmap(void* addr, std::size_t length) {
    int result = __real_munmap(addr, length);
    if (result == 0) {
        std::size_t tracked = mmap_untrack(addr);
        if (tracked > 0)
            par::memory::detail::onFree(static_cast<long long>(tracked));
    }
    return result;
}

// ============================================================================
// sbrk tracking
//
// sbrk(n>0) grows the heap by n bytes — treated as allocation.
// sbrk(n<0) shrinks the heap — treated as deallocation.
// sbrk(0) queries the current break — no tracking.
// ============================================================================

extern "C" void* __wrap_sbrk(intptr_t increment) {
    if (increment > 0) {
        if (!par::memory::detail::tryAlloc(static_cast<long long>(increment))) {
            errno = ENOMEM;
            return reinterpret_cast<void*>(static_cast<intptr_t>(-1));
        }
    }

    void* result = __real_sbrk(increment);

    if (result == reinterpret_cast<void*>(static_cast<intptr_t>(-1))) {
        if (increment > 0)
            par::memory::detail::onFree(static_cast<long long>(increment));
    } else if (increment < 0) {
        par::memory::detail::onFree(static_cast<long long>(-increment));
    }

    return result;
}

#else // _WIN32

// --- Windows aligned allocators ---

extern "C" void* __wrap__aligned_malloc(std::size_t size, std::size_t alignment) {
    return aligned_alloc_impl(alignment, size);
}

extern "C" void __wrap__aligned_free(void* p) {
    __wrap_free(p);  // unified free handles both tracked and foreign
}

extern "C" void* __wrap__aligned_realloc(void* p, std::size_t size, std::size_t alignment) {
    if (!p) return aligned_alloc_impl(alignment, size);
    if (size == 0) { __wrap_free(p); return nullptr; }

    if (!is_our_allocation(p)) {
        // Foreign pointer — can't resize tracked, allocate fresh
        void* new_p = aligned_alloc_impl(alignment, size);
        if (!new_p) return nullptr;
        // Best-effort copy (we don't know original size, use new_size as upper bound)
        std::memcpy(new_p, p, size);
        __real_free(p);
        return new_p;
    }

    void* new_p = aligned_alloc_impl(alignment, size);
    if (!new_p) return nullptr;

    auto* old_meta = get_meta(p);
    std::size_t old_data = old_meta->total - static_cast<std::size_t>(
        static_cast<char*>(p) - static_cast<char*>(old_meta->raw_ptr));
    std::memcpy(new_p, p, old_data < size ? old_data : size);
    __wrap_free(p);
    return new_p;
}

#endif // _WIN32

// ============================================================================
// C++ operator new/delete overrides (regular)
//
// Use __real_malloc/__real_free directly to avoid double-counting
// through __wrap_malloc/__wrap_free. Meta header layout is identical.
// ============================================================================

void* operator new(std::size_t size) {
    std::size_t total = size + sizeof(Meta);
    par::memory::detail::onAlloc(static_cast<long long>(total));  // throws bad_alloc on limit

    void* raw = __real_malloc(total);
    if (!raw) {
        par::memory::detail::onFree(static_cast<long long>(total));
        throw std::bad_alloc();
    }

    auto* meta = static_cast<Meta*>(raw);
    stamp_meta(meta, raw, total);
    return reinterpret_cast<char*>(raw) + sizeof(Meta);
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try { return ::operator new(size); }
    catch (...) { return nullptr; }
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    try { return ::operator new(size); }
    catch (...) { return nullptr; }
}

// Unified delete — checks magic to handle both tracked and foreign pointers.
// Foreign pointers (from libstdc++ out-of-line code or libc) pass through safely.
void operator delete(void* p) noexcept {
    if (!p) return;

    if (!is_our_allocation(p)) {
        __real_free(p);  // foreign pointer — pass through
        return;
    }

    auto* meta = get_meta(p);
    par::memory::detail::onFree(static_cast<long long>(meta->total));
    __real_free(meta->raw_ptr);
}

void operator delete[](void* p) noexcept {
    ::operator delete(p);
}

void operator delete(void* p, std::size_t) noexcept {
    ::operator delete(p);
}

void operator delete[](void* p, std::size_t) noexcept {
    ::operator delete(p);
}

void operator delete(void* p, const std::nothrow_t&) noexcept {
    ::operator delete(p);
}

void operator delete[](void* p, const std::nothrow_t&) noexcept {
    ::operator delete(p);
}

// ============================================================================
// C++17 aligned operator new/delete overrides
//
// When a type has alignas > __STDCPP_DEFAULT_NEW_ALIGNMENT__ (typically 16),
// the compiler automatically uses these overloads. Without overriding them,
// the default implementation in libstdc++ calls aligned_alloc/memalign
// INTERNALLY (not through --wrap), bypassing tracking entirely.
//
// Uses the same over-allocate + align strategy as aligned_alloc_impl,
// but with __real_malloc (not __wrap_malloc) to avoid double-counting.
// Unified Meta layout: delete reads Meta at ptr - sizeof(Meta).
// ============================================================================

#ifdef __cpp_aligned_new

void* operator new(std::size_t size, std::align_val_t al) {
    auto alignment = static_cast<std::size_t>(al);
    if (alignment < sizeof(Meta)) alignment = sizeof(Meta);
    std::size_t total = size + alignment + sizeof(Meta);

    par::memory::detail::onAlloc(static_cast<long long>(total));  // throws bad_alloc

    void* raw = __real_malloc(total);
    if (!raw) {
        par::memory::detail::onFree(static_cast<long long>(total));
        throw std::bad_alloc();
    }

    std::uintptr_t base    = reinterpret_cast<std::uintptr_t>(raw) + sizeof(Meta);
    std::uintptr_t aligned = (base + alignment - 1) & ~(alignment - 1);

    auto* meta = reinterpret_cast<Meta*>(aligned) - 1;
    stamp_meta(meta, raw, total);
    return reinterpret_cast<void*>(aligned);
}

void* operator new[](std::size_t size, std::align_val_t al) {
    return ::operator new(size, al);
}

void* operator new(std::size_t size, std::align_val_t al, const std::nothrow_t&) noexcept {
    try { return ::operator new(size, al); }
    catch (...) { return nullptr; }
}

void* operator new[](std::size_t size, std::align_val_t al, const std::nothrow_t&) noexcept {
    try { return ::operator new(size, al); }
    catch (...) { return nullptr; }
}

// Aligned delete — delegates to regular delete (unified Meta + magic check)
void operator delete(void* p, std::align_val_t) noexcept {
    ::operator delete(p);
}

void operator delete[](void* p, std::align_val_t al) noexcept {
    ::operator delete(p, al);
}

void operator delete(void* p, std::size_t, std::align_val_t al) noexcept {
    ::operator delete(p, al);
}

void operator delete[](void* p, std::size_t sz, std::align_val_t al) noexcept {
    ::operator delete(p, sz, al);
}

void operator delete(void* p, std::align_val_t al, const std::nothrow_t&) noexcept {
    ::operator delete(p, al);
}

void operator delete[](void* p, std::align_val_t al, const std::nothrow_t&) noexcept {
    ::operator delete(p, al);
}

#endif // __cpp_aligned_new
