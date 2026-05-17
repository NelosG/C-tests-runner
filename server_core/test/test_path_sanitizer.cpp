// Unit tests for path_sanitizer::sanitize.
//
// The sanitizer collapses absolute Unix / Windows paths embedded in arbitrary
// text into "<...>/basename" so server topology never leaks via build output
// or stderr that reaches the orchestrator / student.

#include <path_sanitizer.h>
#include <gtest/gtest.h>

using path_sanitizer::sanitize;

// -----------------------------------------------------------------------------
// No-op cases - must not garble normal text
// -----------------------------------------------------------------------------

TEST(PathSanitizer, plain_text_without_paths_is_unchanged) {
    EXPECT_EQ(sanitize(""), "");
    EXPECT_EQ(sanitize("nothing to see here"), "nothing to see here");
    EXPECT_EQ(sanitize("foo bar baz"), "foo bar baz");
}

TEST(PathSanitizer, relative_paths_are_preserved) {
    // No leading separator and no drive letter - must NOT trigger collapsing.
    EXPECT_EQ(sanitize("src/main.cpp"), "src/main.cpp");
    EXPECT_EQ(sanitize("a/b/c"), "a/b/c");
    EXPECT_EQ(sanitize("./relative/path.txt"), "./relative/path.txt");
}

// -----------------------------------------------------------------------------
// Unix absolute paths
// -----------------------------------------------------------------------------

TEST(PathSanitizer, unix_absolute_path_collapses_to_basename) {
    EXPECT_EQ(sanitize("/tmp/foo/bar.cpp"), "<...>/bar.cpp");
    EXPECT_EQ(sanitize("/opt/ctr/server"), "<...>/server");
}

TEST(PathSanitizer, unix_path_at_start_of_string) {
    // Boundary condition: i == 0 must trigger sanitisation.
    EXPECT_EQ(
        sanitize("/usr/local/lib/foo.so error"),
        "<...>/foo.so error"
    );
}

TEST(PathSanitizer, unix_path_inside_quoted_message) {
    EXPECT_EQ(
        sanitize("could not open \"/var/run/app.pid\" for writing"),
        "could not open \"<...>/app.pid\" for writing"
    );
}

TEST(PathSanitizer, unix_path_in_parenthesised_diagnostic) {
    // gcc-style diagnostic: foo (/abs/path/file.cpp:42)
    EXPECT_EQ(
        sanitize("error (/tmp/build/main.cpp:10)"),
        "error (<...>/main.cpp:10)"
    );
}

TEST(PathSanitizer, lone_slash_is_not_a_path) {
    // Sanitiser requires a non-space, non-newline char after '/', so a bare '/'
    // (no following char to inspect) is left untouched.
    EXPECT_EQ(sanitize("/"), "/");
    EXPECT_EQ(sanitize(" /"), " /");
    EXPECT_EQ(sanitize("/ foo"), "/ foo");
}

TEST(PathSanitizer, trailing_slash_inside_path_is_stripped_before_basename) {
    // When the absolute path ends with one or more slashes, they are trimmed
    // before the basename is extracted. "/tmp/foo/" must yield ".../foo".
    EXPECT_EQ(sanitize("/tmp/foo/ rest"), "<...>/foo rest");
}

TEST(PathSanitizer, multiple_unix_paths_in_one_line) {
    EXPECT_EQ(
        sanitize("from /a/b/x.cpp to /c/d/y.cpp"),
        "from <...>/x.cpp to <...>/y.cpp"
    );
}

// -----------------------------------------------------------------------------
// Windows absolute paths
// -----------------------------------------------------------------------------

TEST(PathSanitizer, windows_path_with_backslashes) {
    EXPECT_EQ(
        sanitize("C:\\Users\\bob\\build\\foo.obj"),
        "<...>/foo.obj"
    );
}

TEST(PathSanitizer, windows_path_with_forward_slashes) {
    EXPECT_EQ(
        sanitize("C:/Users/bob/build/foo.cpp"),
        "<...>/foo.cpp"
    );
}

TEST(PathSanitizer, windows_path_in_quotes) {
    EXPECT_EQ(
        sanitize("cannot open 'D:\\Temp\\out.txt' for write"),
        "cannot open '<...>/out.txt' for write"
    );
}

TEST(PathSanitizer, drive_letter_only_is_not_a_path) {
    // "C:" with nothing after fails the (i+2 < size && '\\' || '/') check.
    EXPECT_EQ(
        sanitize("variable C: not a path"),
        "variable C: not a path"
    );
}

// -----------------------------------------------------------------------------
// Boundary handling - only at recognised starting positions
// -----------------------------------------------------------------------------

TEST(PathSanitizer, slash_in_middle_of_word_is_not_a_path) {
    // No boundary before the '/', so we must not eat half a token.
    EXPECT_EQ(
        sanitize("https://example.com/path"),
        "https://example.com/path"
    );
}

TEST(PathSanitizer, unix_path_after_equals_is_recognised) {
    // '=' is a recognised boundary (build var assignments etc).
    EXPECT_EQ(
        sanitize("CMAKE_BUILD_DIR=/tmp/build"),
        "CMAKE_BUILD_DIR=<...>/build"
    );
}

TEST(PathSanitizer, dash_is_a_boundary_only_if_directly_before_slash) {
    // '-' is in the boundary set, so a path that *immediately* follows '-'
    // (e.g. a `-/abs/path` argument) gets collapsed; "-I/abs/path" does NOT,
    // because the char right before '/' is 'I', which is not a boundary.
    EXPECT_EQ(sanitize("-/usr/include/foo"), "-<...>/foo");
    EXPECT_EQ(sanitize("-I/usr/include/foo"), "-I/usr/include/foo");
}
