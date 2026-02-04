#pragma once
#include <functional>
#include <string>
#include <utility>

class Test {
    std::function<std::pair<bool, std::string>()> test;

    public:
        Test(
            std::string name,
            const std::function<std::pair<bool, std::string>()>& test
        ) : test(test), name(std::move(name)) {};
        std::string name;

        std::pair<bool, std::string> operator()() const {
            return test();
        }
};
