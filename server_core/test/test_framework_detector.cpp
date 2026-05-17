// Unit tests for FrameworkDetector - scans student CMake files to determine
// which parallel framework (openmp / parlay / cilk) the solution uses.

#include <framework_detector.h>
#include <gtest/gtest.h>

#include "test_temp_dir.h"


namespace {

    FrameworkDetector::Result detect(
        const std::string& cmakelists,
        const std::vector<std::string>& allowed
    ) {
        TempDir dir;
        dir.write_file("CMakeLists.txt", cmakelists);
        return FrameworkDetector::detect(dir.path(), allowed);
    }

} // namespace

// -----------------------------------------------------------------------------
// Missing directory / no CMake files
// -----------------------------------------------------------------------------

TEST(FrameworkDetector, missing_dir_reports_error) {
    auto r = FrameworkDetector::detect("path/that/does/not/exist", {"openmp"});
    EXPECT_FALSE(r.ok);
    EXPECT_FALSE(r.error_message.empty());
}

TEST(FrameworkDetector, empty_dir_reports_error) {
    TempDir dir;  // no CMakeLists.txt
    auto r = FrameworkDetector::detect(dir.path(), {"openmp"});
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error_message.find("No CMakeLists.txt"), std::string::npos);
}

// -----------------------------------------------------------------------------
// OpenMP detection - three independent markers
// -----------------------------------------------------------------------------

TEST(FrameworkDetector, openmp_via_find_package) {
    auto r = detect("find_package(OpenMP REQUIRED)", {"openmp"});
    ASSERT_TRUE(r.ok) << r.error_message;
    EXPECT_EQ(r.framework, "openmp");
}

TEST(FrameworkDetector, openmp_via_namespaced_target) {
    auto r = detect(
        "target_link_libraries(sol PRIVATE OpenMP::OpenMP_CXX)",
        {"openmp"}
    );
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.framework, "openmp");
}

TEST(FrameworkDetector, openmp_via_parallel_lib_marker) {
    // parallel_lib is the engine-provided OMP wrapper, so its presence implies openmp.
    auto r = detect("target_link_libraries(sol PRIVATE parallel_lib)", {"openmp"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.framework, "openmp");
}

TEST(FrameworkDetector, openmp_find_package_case_insensitive) {
    auto r = detect("FIND_PACKAGE(openmp)", {"openmp"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.framework, "openmp");
}

// -----------------------------------------------------------------------------
// Parlay detection
// -----------------------------------------------------------------------------

TEST(FrameworkDetector, parlay_via_bare_token) {
    auto r = detect("target_link_libraries(sol PRIVATE parlay)", {"parlay"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.framework, "parlay");
}

TEST(FrameworkDetector, parlay_via_namespaced_target) {
    auto r = detect("target_link_libraries(sol PRIVATE parlay::parlay)", {"parlay"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.framework, "parlay");
}

// -----------------------------------------------------------------------------
// Cilk detection
// -----------------------------------------------------------------------------

TEST(FrameworkDetector, cilk_via_compile_flag) {
    auto r = detect("target_compile_options(sol PRIVATE -fopencilk)", {"cilk"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.framework, "cilk");
}

TEST(FrameworkDetector, cilk_via_compile_flag_msvc_style) {
    // Either - or / prefix should match.
    auto r = detect("target_compile_options(sol PRIVATE /fopencilk)", {"cilk"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.framework, "cilk");
}

TEST(FrameworkDetector, cilk_flag_in_word_boundary_only) {
    // The flag must be preceded by a boundary char, otherwise it's not a flag.
    // 'foo-fopencilk' is part of an identifier, not a real -fopencilk flag.
    auto r = detect("set(VAR \"-fopencilkbar\")", {"cilk"});
    EXPECT_FALSE(r.ok)
        << "regex requires \\b after the flag - partial match must not trigger";
}

// -----------------------------------------------------------------------------
// "none" - sequential code
// -----------------------------------------------------------------------------

TEST(FrameworkDetector, no_markers_with_empty_allowed_is_none) {
    auto r = detect("# just a comment\nadd_library(sol src.cpp)", {});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.framework, "none");
}

TEST(FrameworkDetector, no_markers_with_none_in_allowed_is_none) {
    auto r = detect("add_library(sol src.cpp)", {"none"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.framework, "none");
}

TEST(FrameworkDetector, no_markers_but_parallel_required_rejects) {
    auto r = detect("add_library(sol src.cpp)", {"openmp"});
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error_message.find("No parallel framework"), std::string::npos);
}

// -----------------------------------------------------------------------------
// Ambiguity & allow-list filtering
// -----------------------------------------------------------------------------

TEST(FrameworkDetector, multiple_frameworks_rejected) {
    auto r = detect(
        R"(
        find_package(OpenMP REQUIRED)
        target_compile_options(sol PRIVATE -fopencilk)
    )",
        {"openmp", "cilk"}
    );
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error_message.find("Multiple"), std::string::npos);
}

TEST(FrameworkDetector, detected_but_not_in_allowed_rejects) {
    auto r = detect("find_package(OpenMP REQUIRED)", {"parlay"});
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error_message.find("not in allowed list"), std::string::npos);
}

TEST(FrameworkDetector, detected_but_allowed_is_empty_rejects) {
    // Empty allowed_frameworks == "no parallelism permitted".
    auto r = detect("find_package(OpenMP REQUIRED)", {});
    EXPECT_FALSE(r.ok);
    EXPECT_NE(r.error_message.find("forbids"), std::string::npos);
}

// -----------------------------------------------------------------------------
// Comment stripping + recursive *.cmake collection
// -----------------------------------------------------------------------------

TEST(FrameworkDetector, commented_marker_is_not_a_match) {
    auto r = detect("# find_package(OpenMP)\nadd_library(sol src.cpp)", {"openmp"});
    EXPECT_FALSE(r.ok) << "comment must not trigger detection";
}

TEST(FrameworkDetector, marker_inside_quoted_string_is_still_matched) {
    // The detector only strips line-comments - quoted text is part of the
    // searched corpus. This documents current behaviour (consistent with
    // the CMakeValidator approach).
    auto r = detect("set(MSG \"using parlay\")", {"parlay"});
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(r.framework, "parlay");
}

TEST(FrameworkDetector, recursive_scan_picks_up_dot_cmake_files) {
    TempDir dir;
    dir.write_file("CMakeLists.txt", "add_library(sol src.cpp)\n");
    dir.write_file("cmake/parallel.cmake", "find_package(OpenMP REQUIRED)\n");

    auto r = FrameworkDetector::detect(dir.path(), {"openmp"});
    ASSERT_TRUE(r.ok) << r.error_message;
    EXPECT_EQ(r.framework, "openmp");
}

TEST(FrameworkDetector, recursive_scan_picks_up_nested_cmakelists) {
    TempDir dir;
    dir.write_file("CMakeLists.txt", "add_subdirectory(impl)\n");
    dir.write_file("impl/CMakeLists.txt", "target_compile_options(sol PRIVATE -fopencilk)\n");

    auto r = FrameworkDetector::detect(dir.path(), {"cilk"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.framework, "cilk");
}
