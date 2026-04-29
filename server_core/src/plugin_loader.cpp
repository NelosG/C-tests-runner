#include <dll_utils.h>
#include <filesystem>
#include <log_utils.h>
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
    LOG("PluginLoader") << "Loading plugin: " << plugin_name << "\n";

    void* handle = dll::load(plugin_path);
    if(!handle) {
        LOG_ERR("PluginLoader") << "Failed to load: " << plugin_name
            << " (error: " << dll::last_error() << ")\n";
        return false;
    }

    plugin_handles_.emplace_back(plugin_path, handle);
    LOG("PluginLoader") << "Successfully loaded: " << plugin_name << "\n";
    return true;
}

void PluginLoader::unload_all() {
    for(const auto& [path, handle] : plugin_handles_) {
        LOG("PluginLoader") << "Unload plugin: " << fs::path(path).filename().string() << "\n";
        dll::free(handle);
    }
    plugin_handles_.clear();
}
