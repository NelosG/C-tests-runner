// Unit tests for request_helpers::extract_solution_name - derives a display
// name for the submitted solution from either a local path or a git URL.

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <request_helpers.h>

using request_helpers::extract_solution_name;

// -----------------------------------------------------------------------------
// Empty / missing requests
// -----------------------------------------------------------------------------

TEST(ExtractSolutionName, empty_request_returns_empty) {
    EXPECT_EQ(extract_solution_name(nlohmann::json::object()), "");
}

TEST(ExtractSolutionName, solution_source_without_path_or_url_returns_empty) {
    nlohmann::json req = {{"solutionSource", {{"type", "git"}}}};   // missing url+path
    EXPECT_EQ(extract_solution_name(req), "");
}

// -----------------------------------------------------------------------------
// Local path solutionSource
// -----------------------------------------------------------------------------

TEST(ExtractSolutionName, local_path_returns_basename) {
    nlohmann::json req = {{"solutionSource", {{"path", "/abs/dir/my-solution"}}}};
    EXPECT_EQ(extract_solution_name(req), "my-solution");
}

TEST(ExtractSolutionName, local_path_handles_windows_separators) {
    nlohmann::json req = {{"solutionSource", {{"path", "C:\\workspace\\student-x"}}}};
    // std::filesystem::path::filename() handles both / and \ on Windows.
    auto out = extract_solution_name(req);
    EXPECT_EQ(out, "student-x");
}

TEST(ExtractSolutionName, local_path_with_trailing_slash_yields_empty_basename) {
    // path("foo/").filename() == "" by std::filesystem convention.
    nlohmann::json req = {{"solutionSource", {{"path", "foo/bar/"}}}};
    EXPECT_EQ(extract_solution_name(req), "");
}

// -----------------------------------------------------------------------------
// Git URL solutionSource
// -----------------------------------------------------------------------------

TEST(ExtractSolutionName, git_https_url_returns_last_segment) {
    nlohmann::json req = {
        {
            "solutionSource",
            {{"url", "https://gitlab.example.com/group/project.git"}}
        }
    };
    EXPECT_EQ(extract_solution_name(req), "project");
}

TEST(ExtractSolutionName, git_ssh_url_returns_last_segment) {
    // SSH-style URL: git@host:group/project.git - split on the last '/'.
    nlohmann::json req = {
        {
            "solutionSource",
            {{"url", "git@gitlab.example.com:group/project.git"}}
        }
    };
    EXPECT_EQ(extract_solution_name(req), "project");
}

TEST(ExtractSolutionName, url_without_dot_git_suffix_is_kept_as_is) {
    nlohmann::json req = {
        {
            "solutionSource",
            {{"url", "https://example.com/group/foo"}}
        }
    };
    EXPECT_EQ(extract_solution_name(req), "foo");
}

TEST(ExtractSolutionName, url_with_no_slash_is_returned_verbatim_minus_git) {
    // No '/' -> rfind returns npos -> whole url is taken -> .git is stripped.
    nlohmann::json req = {{"solutionSource", {{"url", "bare.git"}}}};
    EXPECT_EQ(extract_solution_name(req), "bare");
}

TEST(ExtractSolutionName, short_git_suffix_only_stripped_when_longer_than_four) {
    // The implementation requires name.size() > 4 to strip ".git". A name that
    // is literally ".git" (4 chars) must NOT be stripped, otherwise we'd
    // return an empty string from a non-empty URL.
    nlohmann::json req = {{"solutionSource", {{"url", "host/.git"}}}};
    EXPECT_EQ(extract_solution_name(req), ".git");
}

// -----------------------------------------------------------------------------
// path takes precedence over url when both are present
// -----------------------------------------------------------------------------

TEST(ExtractSolutionName, path_wins_over_url_when_both_present) {
    nlohmann::json req = {
        {
            "solutionSource",
            {
                {"path", "local/student"},
                {"url", "https://example.com/different.git"}
            }
        }
    };
    EXPECT_EQ(extract_solution_name(req), "student");
}
