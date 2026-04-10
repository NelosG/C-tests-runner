#pragma once

/**
 * @file framework_detector.h
 * @brief Pick the parallel framework used by a student solution from its CMake files.
 *
 * Scans <solution_dir>/CMakeLists.txt and any *.cmake under solution_dir for
 * framework markers (find_package, link targets, compile options). Picks the
 * single framework that matches; rejects ambiguous or missing matches and
 * frameworks not in the teacher-supplied allow list.
 *
 * Source files (*.cpp/*.h) are intentionally NOT scanned - students must
 * declare framework usage through CMake so the validator and BuildService have
 * a single source of truth.
 */

#include <filesystem>
#include <string>
#include <vector>

class FrameworkDetector {
    public:
        struct Result {
            bool ok = false;
            std::string framework;       ///< "openmp" | "parlay" | "cilk" (only when ok)
            std::string error_message;   ///< human-readable diagnostic (only when !ok)
        };

        /// Analyse CMake files under solution_dir, return the matched framework
        /// or an error if detection is ambiguous, empty, or outside `allowed`.
        static Result detect(
            const std::filesystem::path& solution_dir,
            const std::vector<std::string>& allowed
        );
};
