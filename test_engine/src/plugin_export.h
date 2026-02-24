#pragma once

/**
 * @file plugin_export.h
 * @brief Cross-platform DLL export/import macros for plugin symbols.
 *
 * On Windows, symbols must be explicitly exported (__declspec(dllexport))
 * from the plugin DLL and imported (__declspec(dllimport)) by consumers.
 * On Linux/macOS, default visibility achieves the same effect.
 *
 * Define PLUGIN_EXPORTS when building a plugin DLL on Windows.
 */

#ifdef _WIN32
#ifdef PLUGIN_EXPORTS
#define PLUGIN_API __declspec(dllexport)
#else
#define PLUGIN_API __declspec(dllimport)
#endif
#else
#define PLUGIN_API __attribute__((visibility("default")))
#endif