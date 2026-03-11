#pragma once

/**
 * @file project_utils.h
 * @brief Project root detection utility.
 */

#include <filesystem>


namespace project {

    /// Walk up from exe_path looking for CMakeLists.txt + test_engine/.
    inline std::filesystem::path findRoot(const std::filesystem::path& exe_path) {
        namespace fs = std::filesystem;
        fs::path dir = exe_path.parent_path();
        for(int i = 0; i < 5; ++i) {
            if(fs::exists(dir / "CMakeLists.txt") && fs::is_directory(dir / "test_engine")) {
                return dir;
            }
            if(dir.has_parent_path() && dir.parent_path() != dir) {
                dir = dir.parent_path();
            } else {
                break;
            }
        }
        return {};
    }

} // namespace project