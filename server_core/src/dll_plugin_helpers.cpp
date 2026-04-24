#include "dll_plugin_helpers.h"

#include <log_utils.h>

namespace fs = std::filesystem;


namespace dll_plugin_helpers {

    namespace {
        #ifdef _WIN32
        constexpr const char* SHARED_LIB_EXT = ".dll";
        #else
        constexpr const char* SHARED_LIB_EXT = ".so";
        #endif
    } // namespace

    std::string NamingScheme::name_from_stem(const std::string& stem) const {
        bool prefix_matches = !dll_prefix.empty()
            && stem.size() > dll_prefix.size()
            && stem.compare(0, dll_prefix.size(), dll_prefix) == 0;
        bool suffix_matches = !dll_suffix.empty()
            && stem.size() > dll_suffix.size()
            && stem.compare(
                stem.size() - dll_suffix.size(),
                dll_suffix.size(),
                dll_suffix
            ) == 0;

        if(prefix_matches) return stem.substr(dll_prefix.size());
        if(suffix_matches) return stem.substr(0, stem.size() - dll_suffix.size());
        return stem;
    }

    std::map<std::string, std::string> scan_plugin_dir(
        const fs::path& dir,
        const NamingScheme& scheme,
        const std::string& log_prefix
    ) {
        std::map<std::string, std::string> result;

        std::error_code dir_ec;
        if(!fs::is_directory(dir, dir_ec) || dir_ec) {
            LOG_ERR(log_prefix) << "Plugin dir not found: " << dir.generic_string() << "\n";
            return result;
        }

        std::error_code iter_ec;
        for(auto it = fs::directory_iterator(dir, iter_ec);
            !iter_ec && it != fs::directory_iterator();
            it.increment(iter_ec)) {
            std::error_code op_ec;
            if(!it->is_regular_file(op_ec) || op_ec) continue;
            if(it->path().extension().string() != SHARED_LIB_EXT) continue;

            std::string stem = it->path().stem().string();
            std::string name = scheme.name_from_stem(stem);
            result[name] = it->path().string();
        }

        LOG(log_prefix) << "Found " << result.size()
            << " plugin(s) in " << dir.generic_string() << "\n";
        for(auto& [name, path] : result) {
            LOG(log_prefix) << "  - " << name << " (" << path << ")\n";
        }
        return result;
    }

    LoadedDll load_and_resolve(
        const std::string& dll_path,
        const std::string& create_symbol,
        const std::string& destroy_symbol
    ) {
        LoadedDll out;
        out.handle = dll::load(dll_path);
        if(!out.handle) {
            out.error = "Failed to load DLL: " + dll::last_error();
            return out;
        }

        out.create_sym = dll::get_sym(out.handle, create_symbol.c_str());
        out.destroy_sym = dll::get_sym(out.handle, destroy_symbol.c_str());
        if(!out.create_sym || !out.destroy_sym) {
            out.error = "DLL missing " + create_symbol + " / " + destroy_symbol + " symbols";
            dll::free(out.handle);
            out.handle = nullptr;
            out.create_sym = nullptr;
            out.destroy_sym = nullptr;
            return out;
        }
        return out;
    }

    nlohmann::json effective_config(
        const nlohmann::json& provided,
        const fs::path& config_dir,
        const std::string& fallback_filename
    ) {
        if(!provided.empty() || config_dir.empty()) return provided;
        fs::path cfg_path = config_dir / fallback_filename;
        return config::read_json_file(cfg_path);
    }

} // namespace dll_plugin_helpers
