#pragma once

#include <filesystem>
#include <string>


namespace path_utils {

    /// Display path relative to base with forward slashes. Returns absolute on failure.
    inline std::string rel(const std::filesystem::path& p, const std::filesystem::path& base) {
        if(base.empty() || p.empty()) return p.generic_string();
        try { return std::filesystem::proximate(p, base).generic_string(); } catch(...) { return p.generic_string(); }
    }

} // namespace path_utils
