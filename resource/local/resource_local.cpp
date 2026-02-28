#include <filesystem>
#include <iostream>
#include <register_resource_provider.h>
#include <resource_context.h>
#include <resource_local.h>
#include <stdexcept>

namespace fs = std::filesystem;


namespace {

    /// Resolve path against base_dir with path-traversal check.
/// Mirrors the resolvePath() logic from the old test_runner_service.cpp.
    fs::path resolveWithBase(const std::string& p, const std::string& base_dir) {
        if(p.empty()) return {};
        fs::path fp(p);
        if(fp.is_relative() && !base_dir.empty())
            fp = fs::path(base_dir) / fp;
        auto resolved = fs::weakly_canonical(fp);

        // Path traversal check: resolved path must remain within base_dir
        if(!base_dir.empty()) {
            auto base_canonical = fs::weakly_canonical(fs::path(base_dir));
            auto resolved_str = resolved.string();
            auto base_str = base_canonical.string();
            if(resolved_str.find(base_str) != 0) {
                throw std::runtime_error(
                    "Path traversal blocked: resolved path escapes base directory"
                );
            }
        }
        return resolved;
    }

} // anonymous namespace

LocalResourceProvider::LocalResourceProvider(const ResourceContext& ctx) {
    auto base_dirs = ctx.config.value("baseDirs", nlohmann::json::object());
    base_solutions_ = base_dirs.value("solutions", "");
    base_tests_ = base_dirs.value("tests", "");
}

std::filesystem::path LocalResourceProvider::resolve(const nlohmann::json& descriptor) {
    std::string path_str = descriptor.value("path", "");
    if(path_str.empty()) {
        throw std::runtime_error("[LocalProvider] Descriptor missing 'path' field");
    }

    // Select base dir by _kind ("solution" or "test")
    std::string kind = descriptor.value("_kind", "");
    std::string base_dir;
    if(kind == "solution") {
        base_dir = base_solutions_;
    } else if(kind == "test") {
        base_dir = base_tests_;
    }
    // No kind or unknown kind → use path as-is (no base dir constraint)

    auto result = resolveWithBase(path_str, base_dir);
    if(result.empty()) {
        throw std::runtime_error("[LocalProvider] Could not resolve path: " + path_str);
    }
    return result;
}

bool LocalResourceProvider::validateConfig(const nlohmann::json& config, std::string& error) {
    auto base_dirs = config.value("baseDirs", nlohmann::json::object());
    for(const auto& [key, val] : base_dirs.items()) {
        std::string dir = val.get<std::string>();
        if(!dir.empty() && !fs::is_directory(dir)) {
            error = "baseDirs." + key + " does not exist or is not a directory: " + dir;
            return false;
        }
    }
    return true;
}

REGISTER_RESOURCE_PROVIDER(LocalResourceProvider, "local")