#pragma once

/**
 * @file dll_utils.h
 * @brief Cross-platform dynamic library loading helpers.
 */

#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace dll {

/// Load a dynamic library by path. Returns handle or nullptr on failure.
inline void* load(const std::string& path) {
    #ifdef _WIN32
    return LoadLibraryA(path.c_str());
    #else
    return dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    #endif
}

/// Free (unload) a previously loaded library handle.
inline void free(void* handle) {
    if(!handle) return;
    #ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
    #else
    dlclose(handle);
    #endif
}

/// Resolve a symbol from a loaded library. Returns nullptr on failure.
inline void* getSym(void* handle, const char* sym) {
    #ifdef _WIN32
    return reinterpret_cast<void*>(
        GetProcAddress(static_cast<HMODULE>(handle), sym));
    #else
    return dlsym(handle, sym);
    #endif
}

/// Get the last DLL loading error message.
inline std::string lastError() {
    #ifdef _WIN32
    DWORD err = GetLastError();
    if(err == 0) return "";
    char* buf = nullptr;
    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        nullptr, err, 0,
        reinterpret_cast<LPSTR>(&buf), 0, nullptr
    );
    std::string msg = buf ? buf : "Unknown error";
    LocalFree(buf);
    return msg;
    #else
    const char* err = dlerror();
    return err ? err : "";
    #endif
}

} // namespace dll
