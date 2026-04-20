#include <dll_utils.h>
#include <filesystem>
#include <iostream>
#include <plugin_loader.h>

namespace fs = std::filesystem;

PluginLoader::~PluginLoader() {
    unload_all();
}

PluginLoader::PluginLoader(PluginLoader&& other) noexcept
    : plugin_handles_(std::move(other.plugin_handles_)) {
    other.plugin_handles_.clear();
}

PluginLoader& PluginLoader::operator=(PluginLoader&& other) noexcept {
    if(this != &other) {
        unload_all();
        plugin_handles_ = std::move(other.plugin_handles_);
        other.plugin_handles_.clear();
    }
    return *this;
}

bool PluginLoader::load_plugin(const std::string& plugin_path) {
    std::string plugin_name = fs::path(plugin_path).filename().string();
    std::cout << "[PluginLoader] Loading plugin: " << plugin_name << std::endl;

    void* handle = dll::load(plugin_path);
    if(!handle) {
        std::cerr << "[PluginLoader] Failed to load: " << plugin_name
            << " Error: " << dll::last_error() << std::endl;
        return false;
    }

    plugin_handles_.emplace_back(plugin_path, handle);
    std::cout << "[PluginLoader] Successfully loaded: " << plugin_name << std::endl;
    return true;
}

void PluginLoader::unload_all() {
    for(const auto& [path, handle] : plugin_handles_) {
        std::cout << "[PluginLoader] Unload plugin: " << fs::path(path).filename().string() << std::endl;
        dll::free(handle);
    }
    plugin_handles_.clear();
}
