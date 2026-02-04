#include <filesystem>
#include <iostream>
#include <plugin_export.h>
#include <plugin_loader.h>
#include <test_registry.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace fs = std::filesystem;

PluginLoader::~PluginLoader() {
    unloadAll();
}

PluginLoader::PluginLoader(PluginLoader&& other) noexcept
    : pluginHandles_(std::move(other.pluginHandles_)) {
    other.pluginHandles_.clear();
}

PluginLoader& PluginLoader::operator=(PluginLoader&& other) noexcept {
    if(this != &other) {
        unloadAll();
        pluginHandles_ = std::move(other.pluginHandles_);
        other.pluginHandles_.clear();
    }
    return *this;
}

void* PluginLoader::loadLibrary(const std::string& path) {
    #ifdef _WIN32
    return LoadLibraryA(path.c_str());
    #else
    return dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    #endif
}

void* PluginLoader::getSymbol(void* handle, const std::string& symbol) {
    #ifdef _WIN32
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), symbol.c_str()));
    #else
    return dlsym(handle, symbol.c_str());
    #endif
}

void PluginLoader::unloadLibrary(const std::pair<std::string, void*>& handle) {
    std::cout << "[PluginLoader] Unload plugin: " << handle.first << std::endl;
    if(handle.second) {
        #ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle.second));
        #else
        dlclose(handle);
        #endif
    }
}

std::vector<std::string> PluginLoader::findPluginFiles(const std::string& directory) {
    std::vector<std::string> plugins;

    if(!fs::exists(directory)) {
        std::cerr << "[PluginLoader] Directory does not exist: " << directory << std::endl;
        return plugins;
    }

    #ifdef _WIN32
    const std::string extension = ".dll";
    #else
    const std::string extension = ".so";
    #endif

    for(const auto& entry : fs::directory_iterator(directory)) {
        if(entry.is_regular_file()) {
            const auto& path = entry.path();
            if(path.extension() == extension) {
                plugins.push_back(path.string());
            }
        }
    }

    return plugins;
}

bool PluginLoader::loadPlugin(const std::string& pluginPath) {
    std::cout << "[PluginLoader] Loading plugin: " << pluginPath << std::endl;

    void* handle = loadLibrary(pluginPath);
    if(!handle) {
        #ifdef _WIN32
        std::cerr << "[PluginLoader] Failed to load: " << pluginPath
            << " Error: " << GetLastError() << std::endl;
        #else
        std::cerr << "[PluginLoader] Failed to load: " << pluginPath
            << " Error: " << dlerror() << std::endl;
        #endif
        return false;
    }

    // Ищем функцию регистрации тестов
    auto registerFunc = reinterpret_cast<RegisterPluginFunc>(
        getSymbol(handle, "register_plugin_tests")
    );

    if(!registerFunc) {
        std::cerr << "[PluginLoader] Function 'register_plugin_tests' not found in: "
            << pluginPath << std::endl;
        unloadLibrary({pluginPath, handle});
        return false;
    }

    // Вызываем функцию регистрации - она добавит тесты в TestRegistry
    registerFunc(TestRegistry::instance());

    pluginHandles_.emplace_back(pluginPath, handle);

    std::cout << "[PluginLoader] Successfully loaded: " << pluginPath << std::endl;
    return true;
}

std::vector<std::string> PluginLoader::getLoadedPlugins() const {
    std::vector<std::string> paths;
    for(const auto& [path, handle] : pluginHandles_) {
        paths.emplace_back(path);
    }
    return paths;
}

size_t PluginLoader::loadPluginsFromDirectory(const std::string& directory) {
    std::cout << "[PluginLoader] Scanning directory: " << directory << std::endl;

    auto pluginFiles = findPluginFiles(directory);
    size_t loaded = 0;

    for(const auto& pluginPath : pluginFiles) {
        if(loadPlugin(pluginPath)) {
            ++loaded;
        }
    }

    std::cout << "[PluginLoader] Loaded " << loaded << " of "
        << pluginFiles.size() << " plugins" << std::endl;
    return loaded;
}

void PluginLoader::unloadAll() {
    for(const auto& handle : pluginHandles_) {
        unloadLibrary(handle);
    }
    pluginHandles_.clear();
}
