#include <filesystem>
#include <http_client.h>
#include <iostream>
#include <plugin_loader.h>
#include <test_engine.h>
#include <test_registry.h>
#include <test_scenario_result_converter.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

/**
 * Получает путь к директории с плагинами
 * По умолчанию ищет папку "plugins" рядом с исполняемым файлом
 */
std::string getPluginsDirectory(const std::string& execPath) {
    fs::path execDir = fs::path(execPath).parent_path();
    fs::path pluginsDir = execDir / "plugins";
    return pluginsDir.string();
}

int main(int argc, char** argv) {
    // Определяем директорию с плагинами
    std::string pluginsDir;

    if(argc > 1) {
        // Путь к плагинам можно передать как аргумент
        pluginsDir = argv[1];
    } else {
        // По умолчанию - папка plugins рядом с exe
        pluginsDir = getPluginsDirectory(argv[0]);
    }

    std::cout << "=== C++ Test Runner with Plugin Support ===" << std::endl;
    std::cout << "Looking for plugins in: " << pluginsDir << std::endl;

    // Загружаем все плагины из директории
    PluginLoader loader;
    size_t loadedCount = loader.loadPluginsFromDirectory(pluginsDir);

    if(loadedCount == 0) {
        std::cerr << "Warning: No plugins loaded! Tests may not be available." << std::endl;
        std::cerr << "Make sure .so/.dll files are in: " << pluginsDir << std::endl;
    }

    // Выводим информацию о загруженных плагинах
    std::cout << "\nLoaded plugins:" << std::endl;
    for(const auto& path : loader.getLoadedPlugins()) {
        std::cout << "  - " << path << std::endl;
    }

    // Проверяем зарегистрированные тесты
    const auto& tests = TestRegistry::instance().all();
    std::cout << "\nRegistered test scenarios: " << tests.size() << std::endl;
    for(const auto& test : tests) {
        std::cout << "  - " << test->name() << std::endl;
    }
    std::cout << std::endl;

    // Запускаем тесты
    TestEngine test_engine;

    nlohmann::json full_report;
    full_report["omp_threads"] = test_engine.omp_threads;
    auto details = nlohmann::json::object();
    auto scenarios = nlohmann::json::array();

    const auto results = test_engine.executeCorrectness();

    const auto& converter = TestScenarioResultConverter::instance();

    for(const auto& result : results) {
        scenarios.push_back(converter.to_json(result));
    }
    details["scenarios"] = scenarios;
    full_report["details"] = details;

    std::cout << full_report.dump(4) << '\n';

    loader.unloadAll();
    // Отправляем результаты (опционально)
    try {
        const HttpClient client("http://localhost:8080/");
        client.sendJson(full_report);
    } catch(const std::exception& e) {
        std::cerr << "Warning: Could not send results to server: " << e.what() << std::endl;
    }

    return 0;
}
