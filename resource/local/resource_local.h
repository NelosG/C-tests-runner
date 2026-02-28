#pragma once

/**
 * @file resource_local.h
 * @brief Local filesystem resource provider.
 *
 * Resolves descriptors { "path": "...", "_kind": "solution"|"test" }
 * against configured base directories with path-traversal protection.
 *
 * Config (resource-local.json):
 * {
 *   "baseDirs": {
 *     "solutions": "",   // empty = no base dir (absolute or CWD-relative)
 *     "tests": ""
 *   }
 * }
 *
 * "_kind" is an internal field injected by TestRunnerService to select
 * baseDirs.solutions or baseDirs.tests as the base for relative paths.
 */

#include <filesystem>
#include <resource_context.h>
#include <resource_provider.h>
#include <string>

class LocalResourceProvider : public ResourceProvider {
    public:
        explicit LocalResourceProvider(const ResourceContext& ctx);

        std::string name() const override { return "local"; }

        std::filesystem::path resolve(const nlohmann::json& descriptor) override;

        bool validateConfig(const nlohmann::json& config, std::string& error) override;

    private:
        std::string base_solutions_;
        std::string base_tests_;
};