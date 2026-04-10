#pragma once

/**
 * @file cmake_validator.h
 * @brief Validates student CMakeLists.txt for forbidden dependency usage.
 */

#include <filesystem>
#include <string>
#include <vector>

class CMakeValidator {
    public:
        struct Result {
            bool valid = true;
            std::vector<std::string> violations;
        };

        /**
         * @brief Scan student CMakeLists.txt for forbidden dependency usage.
         *
         * Checks: find_package, find_library, FetchContent_Declare,
         * ExternalProject_Add, target_link_libraries.
         *
         * @param cmake_file Path to the CMakeLists.txt file.
         * @param allowed_packages Case-insensitive package/library names that ARE allowed.
         * @return Violations for any unauthorized dependency usage.
         */
        static Result validate(
            const std::filesystem::path& cmake_file,
            const std::vector<std::string>& allowed_packages
        );
};
