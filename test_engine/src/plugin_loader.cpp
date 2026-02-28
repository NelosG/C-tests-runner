#include <dll_utils.h>
#include <filesystem>
#include <iostream>
#include <plugin_loader.h>
#include <test_registry.h>

namespace fs = std::filesystem;

PluginLoader::~PluginLoader() {
    unloadAll();
}

PluginLoader::PluginLoader(PluginLoader&& other) noexcept
    : plugin_handles_(std::move(other.plugin_handles_)) {
    other.plugin_handles_.clear();
}

PluginLoader& PluginLoader::operator=(PluginLoader&& other) noexcept {
    if(this != &other) {
        unloadAll();
        plugin_handles_ = std::move(other.plugin_handles_);
        other.plugin_handles_.clear();
    }
    return *this;
}

std::vector<std::string> PluginLoader::findPluginFiles(const std::string& directory) {
    std::vector<std::string> plugins;

    if(!fs::exists(directory)) {
        std::cerr << "[PluginLoader] Directory does not exist: " << fs::path(directory).filename().string() <<
            std::endl;
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

bool PluginLoader::loadPlugin(const std::string& plugin_path) {
    std::string plugin_name = fs::path(plugin_path).filename().string();
    std::cout << "[PluginLoader] Loading plugin: " << plugin_name << std::endl;

    void* handle = dll::load(plugin_path);
    if(!handle) {
        std::cerr << "[PluginLoader] Failed to load: " << plugin_name
            << " Error: " << dll::lastError() << std::endl;
        return false;
    }

    plugin_handles_.emplace_back(plugin_path, handle);

    std::cout << "[PluginLoader] Successfully loaded: " << plugin_name << std::endl;
    return true;
}

std::vector<std::string> PluginLoader::getLoadedPlugins() const {
    std::vector<std::string> paths;
    for(const auto& [path, handle] : plugin_handles_) {
        paths.emplace_back(path);
    }
    return paths;
}

size_t PluginLoader::loadPluginsFromDirectory(const std::string& directory) {
    std::cout << "[PluginLoader] Scanning directory: " << fs::path(directory).filename().string() << std::endl;

    auto plugin_files = findPluginFiles(directory);
    size_t loaded = 0;

    for(const auto& plugin_path : plugin_files) {
        if(loadPlugin(plugin_path)) {
            ++loaded;
        }
    }

    std::cout << "[PluginLoader] Loaded " << loaded << " of "
        << plugin_files.size() << " plugins" << std::endl;
    return loaded;
}

void PluginLoader::unloadAll() {
    // Clear registry BEFORE closing handles to avoid dangling vtable pointers.
    TestRegistry::instance().clear();

    for(const auto& [path, handle] : plugin_handles_) {
        std::cout << "[PluginLoader] Unload plugin: " << fs::path(path).filename().string() << std::endl;
        dll::free(handle);
    }
    plugin_handles_.clear();
}