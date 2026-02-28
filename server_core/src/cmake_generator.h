#pragma once

/**
 * @file cmake_generator.h
 * @brief Generates CMakeLists.txt content for test/solution builds.
 *
 * Centralizes all CMake generation logic previously scattered across BuildService.
 * Key deduplication: importedLibrary() replaces 4 repeated imported-target blocks.
 */

#include <string>

class CMakeGenerator {
    public:
        struct Config {
            std::string engine_lib_path;
            std::string engine_include_path;
            std::string parallel_lib_path;
            std::string parallel_include_path;
            std::string memory_inject_path;  ///< Path to memory_limit_inject.cpp for student DLL builds.
        };

        explicit CMakeGenerator(Config config);

        // Reusable building blocks
        std::string importedLibrary(
            const std::string& name,
            const std::string& lib_path,
            const std::string& include_path
        ) const;
        std::string openmpLink(const std::string& target_name) const;
        static std::string cmakeHeader(const std::string& project_name);

        // Composite generators
        std::string testWrapperCMakeLists(
            const std::string& test_dir,
            const std::string& student_lib_dir,
            const std::string& student_include_dir,
            bool force_auto_generate = false
        ) const;
        std::string solutionWrapperCMakeLists() const;
        static std::string defaultSolutionCMakeLists();
        static std::string networkBlockScript();

    private:
        Config config_;
};