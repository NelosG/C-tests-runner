#pragma once
#include <plugin_export.h>
#include <test_registry.h>

/**
 * REGISTER_TEST - макрос для регистрации тестового класса
 * 
 * Использование (в конце .cpp файла):
 *     REGISTER_TEST(MyTestClass)
 * 
 * Создает статический объект, который при инициализации
 * регистрирует экземпляр тестового класса в TestRegistry.
 */
#define REGISTER_TEST(TestClass)                                    \
namespace {                                                         \
    struct TestClass##_registrar {                                  \
        TestClass##_registrar() {                                   \
            TestRegistry::instance().register_test(                 \
                std::make_unique<TestClass>()                       \
            );                                                      \
        }                                                           \
    };                                                              \
    static TestClass##_registrar global_##TestClass##_registrar;    \
}

/**
 *     PLUGIN_REGISTER_TESTS_BEGIN
 *         PLUGIN_REGISTER(CorrectnessTest)
 *         PLUGIN_REGISTER(AnotherTest)
 *     PLUGIN_REGISTER_TESTS_END
 */
// #define PLUGIN_REGISTER_TESTS_BEGIN                                 \
// extern "C" PLUGIN_API void register_plugin_tests(TestRegistry& testRegistry) {
//
// #define PLUGIN_REGISTER(TestClass)                                  \
//     testRegistry.register_test(                                     \
//         std::make_unique<TestClass>()                               \
//     );
//
// #define PLUGIN_REGISTER_TESTS_END                                   \
// }

/**
 * SIMPLE_PLUGIN_REGISTER - упрощенный макрос для одного теста в плагине
 * 
 * Если в DLL только один тестовый класс, можно использовать этот макрос
 * вместо PLUGIN_REGISTER_TESTS_BEGIN/END.
 * 
 * Пример:
 *     SIMPLE_PLUGIN_REGISTER(MyTestClass)
 */
// #define SINGLE_PLUGIN_REGISTER(TestClass)                                       \
// extern "C" PLUGIN_API void register_plugin_tests(TestRegistry& testRegistry) {  \
//     testRegistry.register_test(                                                 \
//         std::make_unique<TestClass>()                                           \
//     );                                                                          \
// }

#define ALL_PLUGIN_REGISTER()                                                   \
extern "C" PLUGIN_API void register_plugin_tests(TestRegistry& testRegistry) {  \
    testRegistry.register_tests(TestRegistry::instance().all());                \
}
