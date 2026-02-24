#pragma once

/**
 * @file build_service.h
 * @brief Dynamically compiles student solutions and test plugins.
 *
 * Workflow: buildSolution() first (student DLL + headers),
 * then buildTests() compiled against the real student DLL.
 *
 * Cross-platform: MinGW on Windows, GCC on Linux.
 */

#include <cmake_generator.h>
#include <string>
#include <vector>

class BuildService {
    public:
        struct BuildConfig {
            std::string engine_lib_path;
            std::string engine_include_path;
            std::string parallel_lib_path;
            std::string parallel_include_path;
            std::string memory_inject_path;  ///< Path to memory_limit_inject.cpp for student DLLs.
            std::string cmake_executable;
            std::string generator;
            std::string exe_dir;
            int correctness_workers = 4;
            long long default_memory_limit_mb = 1024;  ///< Per-job memory limit default (MB), 0 = unlimited.
        };

        /** @brief Result of building test plugins against the student DLL. */
        struct TestBuildResult {
            bool success = false;
            std::string error_message;
            std::string build_output;
            std::vector<std::string> plugin_paths;   ///< Built plugin DLLs.
            std::vector<std::string> dep_lib_paths;   ///< Test dependency libs.
            std::string build_dir;                    ///< Temp dir (for cleanup).
            bool cmake_replaced = false;        ///< True if test CMakeLists was replaced.
            std::string cmake_replaced_reason;  ///< Why the fallback was used.
        };

        /** @brief Result of building only the student solution. */
        struct SolutionBuildResult {
            bool success = false;
            std::string error_message;
            std::string build_output;
            std::string solution_lib_path;
            std::string solution_include_dir;   ///< Student's include path.
            std::string build_dir;  ///< Temp directory (for cleanup).
            bool cmake_replaced = false;        ///< True if student's CMakeLists was replaced.
            std::string cmake_replaced_reason;  ///< Why the fallback was used.
        };

        explicit BuildService(BuildConfig config);

        /** @brief Build test plugins against the real student DLL.
         *  Internet is ALLOWED (no network blocking). */
        TestBuildResult buildTests(
            const std::string& test_dir,
            const std::string& student_lib_dir,
            const std::string& student_include_dir
        );

        /** @brief Build only the student solution. Internet is BLOCKED.
         *  If student's CMakeLists.txt fails (wrong target name, missing deps),
         *  falls back to test_dir/solution-defaults/CMakeLists.txt or auto-generated one. */
        SolutionBuildResult buildSolution(
            const std::string& solution_dir,
            const std::string& test_dir,
            const std::string& job_id
        );

        void cleanup(const std::string& build_dir);

    private:
        BuildConfig config_;
        CMakeGenerator cmake_gen_;
};