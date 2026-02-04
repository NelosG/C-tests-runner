#pragma once
#include <test_registry.h>

// Cross-platform export/import macros
#ifdef _WIN32
    #ifdef PLUGIN_EXPORTS
        #define PLUGIN_API __declspec(dllexport)
    #else
        #define PLUGIN_API __declspec(dllimport)
    #endif
#else
    #define PLUGIN_API __attribute__((visibility("default")))
#endif

// C-style function signature for plugin registration
extern "C" {
    typedef void (*RegisterPluginFunc)(TestRegistry&);
}
