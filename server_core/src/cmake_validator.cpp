#include <cmake_validator.h>

#include <algorithm>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>

CMakeValidator::Result CMakeValidator::validate(
    const std::filesystem::path& cmake_file,
    const std::vector<std::string>& allowed_packages
) {
    Result result;

    std::ifstream file(cmake_file);
    if(!file.is_open()) {
        // If file doesn't exist, nothing to validate
        return result;
    }

    // Strip CMake comments (# outside quoted strings) before matching
    std::string content;
    {
        std::string line;
        while(std::getline(file, line)) {
            bool in_string = false;
            for(size_t i = 0; i < line.size(); ++i) {
                if(line[i] == '"') in_string = !in_string;
                else if(line[i] == '#' && !in_string) {
                    line = line.substr(0, i);
                    break;
                }
            }
            content += line + "\n";
        }
    }

    // Build pre-lowercased allowed set (avoids re-lowering on every call)
    std::set<std::string> allowed_lower;
    for(const auto& pkg : allowed_packages) {
        std::string l = pkg;
        std::transform(l.begin(), l.end(), l.begin(), ::tolower);
        allowed_lower.insert(l);
    }

    auto is_allowed = [&](const std::string& name) -> bool {
        std::string name_lower = name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        return allowed_lower.count(name_lower) > 0;
    };

    // Check commands where the first argument is a package name
    auto check_first_arg = [&](const std::regex& re, const std::string& cmd_name) {
        auto begin = std::sregex_iterator(content.begin(), content.end(), re);
        auto end = std::sregex_iterator();

        for(auto it = begin; it != end; ++it) {
            std::string name = (*it)[1].str();
            if(!is_allowed(name)) {
                result.valid = false;
                result.violations.push_back(
                    cmd_name + "(" + name + ") is not in the allowed library list"
                );
            }
        }
    };

    // --- 0. Blocked commands (security) ---
    {
        std::regex re(R"(execute_process\s*\()", std::regex::icase);
        if(std::regex_search(content, re)) {
            result.valid = false;
            result.violations.push_back("execute_process() is not allowed in student solutions");
        }
    }
    {
        // file(DOWNLOAD ...) / file(UPLOAD ...) - outbound network at configure time.
        std::regex re(R"(file\s*\(\s*(DOWNLOAD|UPLOAD)\b)", std::regex::icase);
        std::smatch m;
        if(std::regex_search(content, m, re)) {
            result.valid = false;
            result.violations.push_back(
                "file(" + m[1].str() + ") is not allowed in student solutions"
            );
        }
    }

    // --- 1. find_package / find_library ---
    check_first_arg(
        std::regex(R"(find_package\s*\(\s*(\w+))", std::regex::icase),
        "find_package"
    );
    check_first_arg(
        std::regex(R"(find_library\s*\(\s*(\w+))", std::regex::icase),
        "find_library"
    );

    // --- 2. FetchContent / ExternalProject ---
    check_first_arg(
        std::regex(R"(FetchContent_Declare\s*\(\s*(\w+))", std::regex::icase),
        "FetchContent_Declare"
    );
    check_first_arg(
        std::regex(R"(ExternalProject_Add\s*\(\s*(\w+))", std::regex::icase),
        "ExternalProject_Add"
    );

    // FetchContent_MakeAvailable can list multiple packages
    {
        std::regex re(R"(FetchContent_MakeAvailable\s*\(([^)]*)\))", std::regex::icase);
        auto begin = std::sregex_iterator(content.begin(), content.end(), re);
        auto end = std::sregex_iterator();
        for(auto it = begin; it != end; ++it) {
            std::istringstream ss((*it)[1].str());
            std::string token;
            while(ss >> token) {
                if(!is_allowed(token)) {
                    result.valid = false;
                    result.violations.push_back(
                        "FetchContent_MakeAvailable(" + token
                        + ") is not in the allowed library list"
                    );
                }
            }
        }
    }

    // --- 3. target_link_libraries - catch unauthorized link targets ---
    {
        static const std::set<std::string> link_keywords = {
            "public",
            "private",
            "interface"
        };
        // System/compiler libs that are always allowed
        static const std::set<std::string> system_libs = {
            "pthread",
            "m",
            "dl",
            "rt",
            "c",
            "stdc++",
            "gcc_s",
            "gomp",
            "atomic"
        };

        std::regex re(R"(target_link_libraries\s*\(([^)]*)\))", std::regex::icase);
        auto begin = std::sregex_iterator(content.begin(), content.end(), re);
        auto end = std::sregex_iterator();

        for(auto it = begin; it != end; ++it) {
            std::istringstream ss((*it)[1].str());
            std::string token;
            bool first = true;  // first token is the target name - skip it

            while(ss >> token) {
                if(first) {
                    first = false;
                    continue;
                }

                // Skip keywords (PUBLIC, PRIVATE, INTERFACE)
                std::string lower = token;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if(link_keywords.count(lower)) continue;

                // Skip imported targets (e.g. OpenMP::OpenMP_CXX, Threads::Threads)
                if(token.find("::") != std::string::npos) continue;

                // Skip generator expressions ($<...>)
                if(token.front() == '$') continue;

                // Skip linker flags (-lfoo, -Wl,...)
                if(token.front() == '-') continue;

                // Skip system libs
                if(system_libs.count(lower)) continue;

                // Validate against allowed packages
                if(!is_allowed(token)) {
                    result.valid = false;
                    result.violations.push_back(
                        "target_link_libraries links \"" + token
                        + "\" which is not in the allowed library list"
                    );
                }
            }
        }
    }

    return result;
}
