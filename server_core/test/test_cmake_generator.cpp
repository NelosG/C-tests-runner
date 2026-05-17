// Unit tests for CMakeGenerator - renders runner_wrapper.cmake.in and
// test_plugin_wrapper.cmake.in into final CMakeLists.txt for the per-job
// student / test-plugin builds.
//
// We isolate the generator from the real engine layout by pointing every
// path field at a TempDir and feeding it our own template files. That lets
// us assert on the exact text the substitutor produces.

#include <cmake_generator.h>
#include <gtest/gtest.h>
#include <stdexcept>

#include "test_temp_dir.h"


namespace {

    bool contains(const std::string& haystack, const std::string& needle) {
        return haystack.find(needle) != std::string::npos;
    }

    /// Build a CMakeGenerator pointed at the given template dir, with all path
    /// fields populated to non-empty stubs so make_absolute() doesn't drop them.
    CMakeGenerator::Config make_config(const std::filesystem::path& tpl_dir) {
        CMakeGenerator::Config cfg;
        cfg.engine_lib_path = "engine/libtest_engine.so";
        cfg.engine_include_path = "engine/include";
        cfg.parallel_lib_path = "engine/libparallel_lib.so";
        cfg.parallel_include_path = "engine/include/parallel_lib";
        cfg.runner_lib_path = "engine/librunner_lib.a";
        cfg.runner_include_path = "engine/include/runner_lib";
        cfg.shadow_omp_dir = "engine/include/shadow_omp";
        cfg.runner_omp_source_path = "engine/runner_lib/omp/src/runner_omp.cpp";
        cfg.runner_parlay_source_path = "engine/runner_lib/parlay/src/runner_parlay.cpp";
        cfg.runner_cilk_source_path = "engine/runner_lib/cilk/src/runner_cilk.cpp";
        cfg.runner_seq_source_path = "engine/runner_lib/seq/src/runner_seq.cpp";
        cfg.parlay_headers_path = "engine/include/parlay";
        cfg.template_dir = tpl_dir.string();
        return cfg;
    }

    /// Write the absolute minimum templates required by the generator.
    /// The substitution check only kicks in for placeholders that look like
    /// @ALL_CAPS_PLACEHOLDER@, so we can keep these tiny and still exercise
    /// the full substitute() path.
    void write_default_templates(const TempDir& dir) {
        dir.write_file(
            "runner_wrapper.cmake.in",
            "cmake_minimum_required(VERSION 3.14)\n"
            "@CILK_FLAGS_BLOCK@\n"
            "set(RUNNER_LIB \"@RUNNER_LIB_PATH@\")\n"
            "set(RUNNER_INC \"@RUNNER_LIB_INCLUDE@\")\n"
            "@FRAMEWORK_BLOCK@\n"
            "@STUDENT_EXTRA_LINK@\n"
            "set(RUNNER_VARIANT @RUNNER_VARIANT@)\n"
            "set(RUNNER_MAIN \"@RUNNER_MAIN_PATH@\")\n"
            "set(TEST_INC \"@TEST_INCLUDE_DIR@\")\n"
            "@EXTRA_LIB_DIRS_BLOCK@\n"
        );

        dir.write_file(
            "test_plugin_wrapper.cmake.in",
            "set(ENGINE_LIB \"@TEST_ENGINE_LIB@\")\n"
            "@TEST_ENGINE_IMPLIB_LINE@\n"
            "@TEST_ENGINE_INTERFACE_FLAGS@\n"
            "set(ENGINE_INCLUDES \"@TEST_ENGINE_INCLUDES@\")\n"
            "set(RUNNER_LIB \"@RUNNER_LIB_PATH@\")\n"
            "set(RUNNER_INC \"@RUNNER_LIB_INCLUDE@\")\n"
            "@PLATFORM_GLOBAL@\n"
            "add_subdirectory(\"@TEACHER_TESTS_DIR@\")\n"
        );
    }

} // namespace

// -----------------------------------------------------------------------------
// Framework dispatch - picks the right runner_<variant> + extras per framework
// -----------------------------------------------------------------------------

class CMakeGeneratorTest : public ::testing::Test {
    protected:
        TempDir tpl_;

        void SetUp() override {
            write_default_templates(tpl_);
        }

        CMakeGenerator make(const CMakeGenerator::Config& cfg_in = {}) {
            auto cfg = make_config(tpl_.path());
            // Caller can override individual paths after construction by mutating cfg_in.
            // We don't use that yet; kept for future tests.
            (void)cfg_in;
            return CMakeGenerator(cfg);
        }
};

TEST_F(CMakeGeneratorTest, openmp_framework_picks_runner_omp) {
    auto gen = make();
    auto out = gen.runner_cmake_lists("openmp", "main.cpp", "include");
    EXPECT_TRUE(contains(out, "RUNNER_VARIANT runner_omp"));
    EXPECT_TRUE(contains(out, "find_package(OpenMP REQUIRED)"));
    EXPECT_TRUE(contains(out, "add_library(parallel_lib SHARED IMPORTED"));
    EXPECT_TRUE(contains(out, "runner_omp.cpp"));
    EXPECT_TRUE(contains(out, "target_link_libraries(runner_omp PUBLIC parallel_lib OpenMP::OpenMP_CXX)"));
    EXPECT_TRUE(contains(out, "target_link_libraries(student_solution PUBLIC parallel_lib OpenMP::OpenMP_CXX)"));
}

TEST_F(CMakeGeneratorTest, parlay_framework_picks_runner_parlay) {
    auto gen = make();
    auto out = gen.runner_cmake_lists("parlay", "main.cpp", "include");
    EXPECT_TRUE(contains(out, "RUNNER_VARIANT runner_parlay"));
    EXPECT_TRUE(contains(out, "runner_parlay.cpp"));
    EXPECT_TRUE(contains(out, "parlay"))
        << "parlay headers path or `parlay` target should appear in the rendered file";
    // No OpenMP for parlay.
    EXPECT_FALSE(contains(out, "find_package(OpenMP"));
}

TEST_F(CMakeGeneratorTest, cilk_framework_picks_runner_cilk_with_flag) {
    auto gen = make();
    auto out = gen.runner_cmake_lists("cilk", "main.cpp", "include");
    EXPECT_TRUE(contains(out, "RUNNER_VARIANT runner_cilk"));
    EXPECT_TRUE(contains(out, "runner_cilk.cpp"));
    EXPECT_TRUE(contains(out, "-fopencilk"));
    // -fopencilk should appear both in global compile/link options block and
    // on the runner_cilk target itself.
    EXPECT_TRUE(contains(out, "add_compile_options(-fopencilk)"));
    EXPECT_TRUE(contains(out, "add_link_options(-fopencilk)"));
}

TEST_F(CMakeGeneratorTest, none_framework_picks_runner_seq) {
    auto gen = make();
    auto out = gen.runner_cmake_lists("none", "main.cpp", "include");
    EXPECT_TRUE(contains(out, "RUNNER_VARIANT runner_seq"));
    EXPECT_TRUE(contains(out, "runner_seq.cpp"));
    EXPECT_FALSE(contains(out, "-fopencilk"));
    EXPECT_FALSE(contains(out, "find_package(OpenMP"));
    EXPECT_FALSE(contains(out, "add_library(parallel_lib"));
}

TEST_F(CMakeGeneratorTest, unknown_framework_throws) {
    auto gen = make();
    EXPECT_THROW(
        gen.runner_cmake_lists("rust", "main.cpp", "include"),
        std::runtime_error
    );
}

// -----------------------------------------------------------------------------
// Path normalization - generator must emit forward slashes regardless of OS
// -----------------------------------------------------------------------------

TEST_F(CMakeGeneratorTest, paths_are_normalized_to_forward_slashes) {
    auto gen = make();
    auto out = gen.runner_cmake_lists("none", "main.cpp", "include");
    // The runner_lib_path stub contains a forward slash already, but on Windows
    // make_absolute() would inject backslashes. Either way, the rendered file
    // must not contain raw backslashes - student-side CMake would mis-escape them.
    EXPECT_EQ(out.find('\\'), std::string::npos)
        << "rendered CMake must use forward slashes (CMake convention)";
}

// -----------------------------------------------------------------------------
// shadow_omp toggle - only emitted for openmp + shadow_omp=true
// -----------------------------------------------------------------------------

TEST_F(CMakeGeneratorTest, shadow_omp_block_present_for_openmp_default) {
    auto gen = make();
    auto out = gen.runner_cmake_lists("openmp", "main.cpp", "include");
    EXPECT_TRUE(contains(out, "shadow_omp"));
}

TEST_F(CMakeGeneratorTest, shadow_omp_block_suppressed_when_disabled) {
    auto gen = make();
    auto out = gen.runner_cmake_lists(
        "openmp",
        "main.cpp",
        "include",
        {},
        false
    );
    EXPECT_FALSE(contains(out, "shadow_omp"));
}

TEST_F(CMakeGeneratorTest, shadow_omp_block_not_emitted_for_non_openmp) {
    auto gen = make();
    for(const std::string& fw : {"parlay", "cilk", "none"}) {
        auto out = gen.runner_cmake_lists(fw, "main.cpp", "include");
        EXPECT_FALSE(contains(out, "shadow_omp")) << "fw=" << fw;
    }
}

// -----------------------------------------------------------------------------
// extra_lib_dirs - appended via target_include_directories(runner ...)
// -----------------------------------------------------------------------------

TEST_F(CMakeGeneratorTest, extra_lib_dirs_render_into_target_includes) {
    auto gen = make();
    auto out = gen.runner_cmake_lists(
        "none",
        "main.cpp",
        "include",
        {"libs/foo", "libs/bar"}
    );
    EXPECT_TRUE(contains(out, "libs/foo"));
    EXPECT_TRUE(contains(out, "libs/bar"));
    EXPECT_TRUE(contains(out, "target_include_directories(runner PRIVATE"));
}

TEST_F(CMakeGeneratorTest, empty_extra_lib_dirs_emits_no_user_include_line) {
    auto gen = make();
    auto out = gen.runner_cmake_lists("none", "main.cpp", "include", {});
    EXPECT_FALSE(contains(out, "target_include_directories(runner PRIVATE"))
        << "with no extra lib dirs and no shadow_omp (framework=none), "
           "the EXTRA_LIB_DIRS_BLOCK should be empty";
}

// -----------------------------------------------------------------------------
// Template error handling
// -----------------------------------------------------------------------------

TEST_F(CMakeGeneratorTest, missing_template_file_throws) {
    // Point template_dir somewhere with no .cmake.in files.
    auto cfg = make_config(tpl_.path());
    cfg.template_dir = (tpl_.path() / "no_such_dir").string();
    CMakeGenerator gen(cfg);
    EXPECT_THROW(
        gen.runner_cmake_lists("none", "main.cpp", "include"),
        std::runtime_error
    );
}

TEST(CMakeGeneratorTemplateValidation, unsubstituted_placeholder_throws) {
    // A template that references @MADE_UP_PLACEHOLDER@ - CMakeGenerator must
    // refuse to render it rather than letting the placeholder slip through.
    TempDir tpl;
    tpl.write_file(
        "runner_wrapper.cmake.in",
        "set(RUNNER_LIB \"@RUNNER_LIB_PATH@\")\n"
        "@MADE_UP_PLACEHOLDER@\n"
    );
    tpl.write_file("test_plugin_wrapper.cmake.in", "");  // unused here

    auto cfg = make_config(tpl.path());
    CMakeGenerator gen(cfg);
    EXPECT_THROW(
        gen.runner_cmake_lists("none", "main.cpp", "include"),
        std::runtime_error
    );
}

TEST(CMakeGeneratorTemplateValidation, at_sign_in_url_is_not_a_placeholder) {
    // Lowercase / mixed text between @ markers shouldn't be treated as a
    // placeholder (it's likely a literal URL or path).
    TempDir tpl;
    tpl.write_file(
        "runner_wrapper.cmake.in",
        "# repo: git@github.com:org/repo.git\n"
        "@CILK_FLAGS_BLOCK@\n"
        "set(RUNNER_LIB \"@RUNNER_LIB_PATH@\")\n"
        "set(RUNNER_INC \"@RUNNER_LIB_INCLUDE@\")\n"
        "@FRAMEWORK_BLOCK@\n"
        "@STUDENT_EXTRA_LINK@\n"
        "set(RUNNER_VARIANT @RUNNER_VARIANT@)\n"
        "set(RUNNER_MAIN \"@RUNNER_MAIN_PATH@\")\n"
        "set(TEST_INC \"@TEST_INCLUDE_DIR@\")\n"
        "@EXTRA_LIB_DIRS_BLOCK@\n"
    );
    tpl.write_file("test_plugin_wrapper.cmake.in", "");

    auto cfg = make_config(tpl.path());
    CMakeGenerator gen(cfg);
    EXPECT_NO_THROW(gen.runner_cmake_lists("none", "main.cpp", "include"))
        << "lowercase/mixed text between @s must be treated as literal, not a placeholder";
}

// -----------------------------------------------------------------------------
// test_plugin_cmake_lists - plugin build wrapper
// -----------------------------------------------------------------------------

TEST_F(CMakeGeneratorTest, plugin_template_includes_test_engine_and_runner_lib) {
    auto gen = make();
    auto out = gen.test_plugin_cmake_lists("teacher/tests", "teacher/include");
    EXPECT_TRUE(contains(out, "libtest_engine.so"));
    EXPECT_TRUE(contains(out, "librunner_lib.a"));
    EXPECT_TRUE(contains(out, "add_subdirectory"));
    EXPECT_TRUE(contains(out, "teacher/tests"))
        << "TEACHER_TESTS_DIR should appear as add_subdirectory target";
}

TEST_F(CMakeGeneratorTest, plugin_template_combines_engine_and_runner_includes) {
    auto gen = make();
    auto out = gen.test_plugin_cmake_lists("teacher/tests", "teacher/include");
    // Engine + runner_lib includes are passed as a ';'-separated CMake list.
    // The exact paths come from cfg fields, so we just check the join token.
    EXPECT_TRUE(contains(out, ";"));
    EXPECT_TRUE(contains(out, "engine/include"));
    EXPECT_TRUE(contains(out, "engine/include/runner_lib"));
}
