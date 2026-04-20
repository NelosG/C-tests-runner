#pragma once

/**
 * @file cmake_generator.h
 * @brief Substitutes the static cmake/runner_wrapper.cmake.in and
 *        cmake/test_plugin_wrapper.cmake.in templates with per-job values.
 *
 * Two entry points: runner_cmake_lists() for the student+runner exe build,
 * and test_plugin_cmake_lists() for the parent-side setup/verify plugin build.
 * Both are thin: each computes a few framework / platform specific blocks as
 * strings and feeds them into the corresponding @PLACEHOLDER@ slots in the
 * static templates that BuildService ships next to the engine binary.
 */

#include <string>
#include <vector>

class CMakeGenerator {
    public:
        struct Config {
            std::string engine_lib_path;
            std::string engine_include_path;
            std::string parallel_lib_path;
            std::string parallel_include_path;
            std::string runner_lib_path;             ///< Path to librunner_lib.a (base, public API in runner.h)
            std::string runner_include_path;         ///< Path to runner_lib/include (single include dir for runner_lib + variants)
            std::string shadow_omp_dir;              ///< Path to parallel_lib/shadow/
            std::string runner_omp_lib_path;         ///< Path to librunner_omp.a
            std::string runner_parlay_lib_path;      ///< Path to librunner_parlay.a
            std::string runner_cilk_lib_path;        ///< Path to librunner_cilk.a
            std::string runner_seq_lib_path;         ///< Path to librunner_seq.a (framework=none)
            std::string parlay_headers_path;         ///< Path to ParlayLib headers (parlay/ subdir)
            std::string template_dir;                ///< Directory containing runner_wrapper.cmake.in etc.
        };

        explicit CMakeGenerator(Config config);

        /// Substitute runner_wrapper.cmake.in with framework / per-job values.
        ///   framework:        "openmp", "parlay", "cilk", or "none"
        ///   runner_main_path: teacher's runner/main.cpp
        ///   test_include_dir: teacher's include/ (interface headers)
        ///   extra_lib_dirs:   teacher's libs/<*>/ dirs (added to runner include path)
        ///   shadow_omp:       include shadow_omp/ on the openmp build (forbids raw <omp.h>)
        std::string runner_cmake_lists(
            const std::string& framework,
            const std::string& runner_main_path,
            const std::string& test_include_dir,
            const std::vector<std::string>& extra_lib_dirs = {},
            bool shadow_omp = true
        ) const;

        /// Substitute test_plugin_wrapper.cmake.in for the teacher's tests/CMakeLists.txt.
        /// Plugins link test_engine + runner_lib via imported targets that carry
        /// the right INTERFACE flags for the current platform. No student code.
        std::string test_plugin_cmake_lists(
            const std::string& test_dir,
            const std::string& test_include_dir
        ) const;

    private:
        Config config_;
};
