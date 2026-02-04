#pragma once

#include <string>
#include <vector>

/**
 * PluginLoader - динамический загрузчик плагинов с тестами
 * 
 * Сканирует указанную директорию на наличие .dll/.so файлов,
 * загружает каждый плагин и вызывает функцию регистрации тестов.
 */
class PluginLoader {
    public:
        PluginLoader() = default;
        ~PluginLoader();

        // Некопируемый
        PluginLoader(const PluginLoader&) = delete;
        PluginLoader& operator=(const PluginLoader&) = delete;

        // Перемещаемый
        PluginLoader(PluginLoader&&) noexcept;
        PluginLoader& operator=(PluginLoader&&) noexcept;

        /**
         * Загружает все плагины из указанной директории
         * @param directory Путь к директории с плагинами
         * @return Количество успешно загруженных плагинов
         */
        size_t loadPluginsFromDirectory(const std::string& directory);

        /**
         * Загружает один плагин по пути
         * @param pluginPath Полный путь к файлу плагина (.dll/.so)
         * @return true если загрузка успешна
         */
        bool loadPlugin(const std::string& pluginPath);

        /**
         * @return Список путей загруженных плагинов
         */
        std::vector<std::string> getLoadedPlugins() const;

        /**
         * Выгружает все загруженные плагины
         */
        void unloadAll();

    private:
        std::vector<std::pair<std::string, void*>> pluginHandles_;

        void* loadLibrary(const std::string& path);
        void* getSymbol(void* handle, const std::string& symbol);
        void unloadLibrary(const std::pair<std::string, void*>& handle);
        std::vector<std::string> findPluginFiles(const std::string& directory);
};
