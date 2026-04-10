#include "framework_detector.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;


namespace {

    std::string read_file(const fs::path& path) {
        std::ifstream f(path, std::ios::binary);
        if(!f) return {};
        std::ostringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    /// Strip CMake comments (# outside quoted strings). Without this, a
    /// `# parlay headers` comment would falsely trigger parlay detection.
    /// Mirrors the same logic in CMakeValidator.
    std::string strip_cmake_comments(const std::string& text) {
        std::string out;
        out.reserve(text.size());
        std::istringstream iss(text);
        std::string line;
        while(std::getline(iss, line)) {
            bool in_string = false;
            for(size_t i = 0; i < line.size(); ++i) {
                if(line[i] == '"') in_string = !in_string;
                else if(line[i] == '#' && !in_string) {
                    line.resize(i);
                    break;
                }
            }
            out += line;
            out += '\n';
        }
        return out;
    }

    /// Collect the contents of every CMake file under `dir`: the top-level
    /// CMakeLists.txt plus any *.cmake / */CMakeLists.txt found recursively.
    /// Comments are stripped before returning.
    std::string collect_cmake_text(const fs::path& dir) {
        std::ostringstream ss;
        std::error_code ec;
        if(!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return {};

        for(auto it = fs::recursive_directory_iterator(dir, ec);
            !ec && it != fs::recursive_directory_iterator();
            it.increment(ec)) {
            std::error_code op_ec;
            if(!it->is_regular_file(op_ec) || op_ec) continue;
            const auto& p = it->path();
            const std::string name = p.filename().string();
            const std::string ext = p.extension().string();
            if(name == "CMakeLists.txt" || ext == ".cmake") {
                ss << read_file(p) << "\n";
            }
        }
        return strip_cmake_comments(ss.str());
    }

    bool match_cilk(const std::string& text) {
        static const std::regex flag(R"((^|[\s"'])[-/]fopencilk\b)");
        return std::regex_search(text, flag);
    }

    bool match_parlay(const std::string& text) {
        // find_package(parlay), parlay::xxx, or bare `parlay` token in
        // target_link_libraries / FetchContent_Declare etc.
        static const std::regex bare(R"(\bparlay\b)");
        return std::regex_search(text, bare);
    }

    bool match_openmp(const std::string& text) {
        static const std::regex find_pkg(
            R"(find_package\s*\(\s*OpenMP\b)",
            std::regex::icase
        );
        static const std::regex namespaced(R"(\bOpenMP::)");
        static const std::regex parallel_lib(R"(\bparallel_lib\b)");
        return std::regex_search(text, find_pkg)
            || std::regex_search(text, namespaced)
            || std::regex_search(text, parallel_lib);
    }

    std::string join_names(const std::vector<std::string>& names) {
        std::string out;
        for(size_t i = 0; i < names.size(); ++i) {
            if(i) out += ", ";
            out += names[i];
        }
        return out;
    }

} // namespace

FrameworkDetector::Result FrameworkDetector::detect(
    const fs::path& solution_dir,
    const std::vector<std::string>& allowed
) {
    Result r;

    const std::string text = collect_cmake_text(solution_dir);
    if(text.empty()) {
        r.error_message = "No CMakeLists.txt or *.cmake files found in solution: "
            + solution_dir.string();
        return r;
    }

    std::vector<std::string> matched;
    if(match_cilk(text)) matched.push_back("cilk");
    if(match_parlay(text)) matched.push_back("parlay");
    if(match_openmp(text)) matched.push_back("openmp");

    // No framework detected -> sequential / std::thread / etc. Engine treats
    // this as the synthetic framework "none". Allowed if `allowed` is empty
    // OR explicitly contains "none"; otherwise rejected (teacher requires a
    // parallel framework).
    if(matched.empty()) {
        const bool none_allowed = allowed.empty()
            || std::find(allowed.begin(), allowed.end(), "none") != allowed.end();
        if(!none_allowed) {
            r.error_message = "No parallel framework detected in student CMake files, "
                "but this assignment requires one of: [" + join_names(allowed) + "]. "
                "Add find_package(OpenMP), parlay marker, or -fopencilk.";
            return r;
        }
        r.ok = true;
        r.framework = "none";
        return r;
    }
    if(matched.size() > 1) {
        r.error_message = "Multiple frameworks detected in student CMake files: "
            + join_names(matched) + ". Pick exactly one.";
        return r;
    }

    const std::string& detected = matched.front();
    if(allowed.empty()) {
        r.error_message = "Framework '" + detected
            + "' detected, but this assignment forbids all parallel frameworks "
            "(allowedFrameworks is empty).";
        return r;
    }
    if(std::find(allowed.begin(), allowed.end(), detected) == allowed.end()) {
        r.error_message = "Framework '" + detected
            + "' is not in allowed list: [" + join_names(allowed) + "]";
        return r;
    }

    r.ok = true;
    r.framework = detected;
    return r;
}
