#pragma once

// Tiny RAII helper used by tests that need an on-disk fixture.
//
// Creates a unique, empty directory under the system temp dir on construction
// and removes it (with everything inside) on destruction. Tests stay isolated
// from each other even when run in parallel via ctest -j N.

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

class TempDir {
    public:
        TempDir() {
            namespace fs = std::filesystem;
            static std::atomic<unsigned> counter{0};
            unsigned id = counter.fetch_add(1, std::memory_order_relaxed);
            auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            path_ = fs::temp_directory_path()
                / ("ctr_ut_" + std::to_string(stamp) + "_" + std::to_string(id));
            fs::create_directories(path_);
        }

        ~TempDir() {
            std::error_code ec;
            std::filesystem::remove_all(path_, ec);
        }

        TempDir(const TempDir&) = delete;
        TempDir& operator=(const TempDir&) = delete;

        const std::filesystem::path& path() const { return path_; }

        /// Write `content` to `<temp>/relative` (creating parent dirs as needed).
        /// Returns the absolute path of the written file.
        std::filesystem::path write_file(
            const std::string& relative,
            const std::string& content
        ) const {
            auto target = path_ / relative;
            std::filesystem::create_directories(target.parent_path());
            std::ofstream out(target, std::ios::binary);
            out << content;
            return target;
        }

    private:
        std::filesystem::path path_;
};
