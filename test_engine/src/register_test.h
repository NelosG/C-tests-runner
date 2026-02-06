#pragma once

/**
 * @file register_test.h
 * @brief Macro for automatic test scenario registration during plugin loading.
 */

#include <test_registry.h>

/**
 * @def REGISTER_TEST(TestClass)
 * @brief Register a TestScenarioExtension subclass in the global TestRegistry.
 *
 * Place this macro at the end of a plugin .cpp file. It creates a static
 * object whose constructor runs during library load (dlopen / LoadLibrary),
 * automatically registering an instance of TestClass.
 *
 * @param TestClass Fully qualified class name deriving from TestScenarioExtension.
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