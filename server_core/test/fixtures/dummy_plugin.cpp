// Tiny fixture shared library used by plugin_loader / adapter_manager /
// resource_manager unit tests. Just needs to be loadable by dlopen/LoadLibrary;
// no exported symbols are required.

extern "C" int dummy_plugin_marker() { return 42; }
