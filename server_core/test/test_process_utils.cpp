// Unit tests for process_utils header - focused on shell_quote.
//
// shell_quote is security-critical: the engine builds shell command lines for
// CMake invocations from teacher/student-controlled paths. A flaw here would
// be a command injection vector. The tests deliberately probe escaping for the
// platform we're compiled for (Windows here; POSIX has its own quoting rules
// guarded by the same ifdef).

#include <gtest/gtest.h>
#include <process_utils.h>

// -----------------------------------------------------------------------------
// shell_quote - output must surround the input with platform-appropriate quotes
// and escape characters that would otherwise terminate the quoted region.
// -----------------------------------------------------------------------------

TEST(ShellQuote, plain_string_is_wrapped_in_quotes) {
    #ifdef _WIN32
    EXPECT_EQ(shell_quote("hello"), "\"hello\"");
    #else
    EXPECT_EQ(shell_quote("hello"), "'hello'");
    #endif
}

TEST(ShellQuote, empty_string_is_safe_to_pass) {
    #ifdef _WIN32
    EXPECT_EQ(shell_quote(""), "\"\"");
    #else
    EXPECT_EQ(shell_quote(""), "''");
    #endif
}

TEST(ShellQuote, path_with_spaces_is_quoted) {
    // A "C:\Program Files\X" path is the canonical reason shell_quote exists.
    auto out = shell_quote("C:\\Program Files\\X");
    #ifdef _WIN32
    EXPECT_EQ(out.front(), '"');
    EXPECT_EQ(out.back(), '"');
    EXPECT_NE(out.find("Program Files"), std::string::npos);
    #else
    EXPECT_EQ(out.front(), '\'');
    EXPECT_EQ(out.back(), '\'');
    #endif
}

#ifdef _WIN32
TEST(ShellQuote, windows_escapes_internal_double_quote) {
    // Input contains a stray double-quote - must be escaped so it can't
    // terminate the quoted region. End-result: " + ab\"cd + "
    EXPECT_EQ(shell_quote("ab\"cd"), "\"ab\\\"cd\"");
}

TEST(ShellQuote, windows_escapes_internal_backslash) {
    // Backslashes must be doubled so a trailing \ doesn't escape the closing ".
    EXPECT_EQ(shell_quote("a\\b"), "\"a\\\\b\"");
}

TEST(ShellQuote, windows_escapes_trailing_backslash) {
    // Specifically the dangerous case: a path ending in \ - escape it.
    EXPECT_EQ(shell_quote("dir\\"), "\"dir\\\\\"");
}

TEST(ShellQuote, windows_injection_attempt_is_neutralised) {
    // Try to break out of the quoted region with `" && rm -rf /`.
    // The escape MUST leave the inner `"` inert.
    auto out = shell_quote("\" && evil");
    // First and last char are wrappers; everything in between must not
    // contain an *unescaped* `"`.
    ASSERT_EQ(out.front(), '"');
    ASSERT_EQ(out.back(), '"');
    std::string inner = out.substr(1, out.size() - 2);
    // Scan for a literal `"` that isn't preceded by `\` - none should exist.
    for(std::size_t i = 0; i < inner.size(); ++i) {
        if(inner[i] == '"') {
            EXPECT_TRUE(i > 0 && inner[i - 1] == '\\')
                << "Unescaped \" at offset " << i << " in: " << out;
        }
    }
}
#else
TEST(ShellQuote, posix_escapes_internal_single_quote_via_close_escape_open) {
    // Standard POSIX trick: 'foo' + \' + 'bar' renders as foo'bar in shell.
    EXPECT_EQ(shell_quote("a'b"), "'a'\\''b'");
}
#endif

// -----------------------------------------------------------------------------
// run_command - at this layer we just make sure the contract holds (exit code,
// captured stdout). Output cap / pipe handling are exercised at integration.
// -----------------------------------------------------------------------------

TEST(RunCommand, captures_stdout_and_returns_zero_for_echo) {
    #ifdef _WIN32
    auto r = run_command("echo hello");
    #else
    auto r = run_command("printf hello");
    #endif
    EXPECT_EQ(r.exit_code, 0);
    EXPECT_FALSE(r.failed());
    EXPECT_NE(r.output.find("hello"), std::string::npos);
}

TEST(RunCommand, nonzero_exit_code_propagates) {
    #ifdef _WIN32
    auto r = run_command("cmd /c exit 7");
    #else
    auto r = run_command("exit 7");
    #endif
    EXPECT_NE(r.exit_code, 0);
    EXPECT_TRUE(r.failed());
}
