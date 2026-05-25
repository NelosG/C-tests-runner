// Shared helpers for classify scenarios. We construct synthetic
// datasets where the label is a deterministic function of features
// (e.g., majority, threshold). Verify checks accuracy on a held-out
// test set.

#pragma once

#include <cstdint>
#include <dataGen.h>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <utility>
#include <vector>

namespace classify_common {

    struct Dataset {
        std::int64_t num_features = 0;
        std::int64_t num_values = 0;
        std::int64_t num_train = 0;
        std::int64_t num_test = 0;
        std::vector<std::uint8_t> train;       // (num_train * num_features) - col 0 is label
        std::vector<std::uint8_t> test;        // (num_test  * num_features) - col 0 unused
        std::vector<std::uint8_t> expected;    // num_test labels
    };

    // Label tracks feature[1] with light noise. Decision tree should
    // pick that feature at the root and recover label with ~90% acc.
    // (Pure XOR-like patterns can't be learned by single-feature splits,
    // which is a real limitation of pbbs's C4.5 - so we avoid those.)
    inline Dataset make_copy_feature(std::int64_t num_features,
                                     std::int64_t num_train,
                                     std::int64_t num_test,
                                     std::int64_t num_values,
                                     std::uint64_t seed = 0)
    {
        Dataset d;
        d.num_features = num_features;
        d.num_train = num_train;
        d.num_test = num_test;
        d.num_values = num_values;
        d.train.assign(num_train * num_features, 0);
        d.test.assign(num_test * num_features, 0);
        d.expected.assign(num_test, 0);
        auto fill = [&](std::vector<std::uint8_t>& M, std::int64_t rows,
                        std::int64_t off) {
            for(std::int64_t r = 0; r < rows; ++r) {
                std::uint8_t f1 = static_cast<std::uint8_t>(
                    dataGen::hash64(seed + off + r*7) % num_values);
                std::uint8_t noise = static_cast<std::uint8_t>(
                    dataGen::hash64(seed + off + r*23) % 10);
                std::uint8_t label = (noise == 0)
                    ? static_cast<std::uint8_t>((f1 + 1) % num_values)
                    : f1;
                M[r * num_features + 0] = label;
                if(num_features > 1) M[r * num_features + 1] = f1;
                for(std::int64_t f = 2; f < num_features; ++f)
                    M[r * num_features + f] = static_cast<std::uint8_t>(
                        dataGen::hash64(seed + off + r*13 + f) % num_values);
            }
        };
        fill(d.train, num_train, 1000);
        fill(d.test,  num_test,  2000);
        for(std::int64_t r = 0; r < num_test; ++r)
            d.expected[r] = d.test[r * num_features + 0];
        return d;
    }

    inline void write_input(TestData& v, const Dataset& d) {
        v.write_object("data", [&](TestData& gd) {
            gd.write_value<std::int64_t>("num_features", d.num_features);
            gd.write_value<std::int64_t>("num_values", d.num_values);
            gd.write_value<std::int64_t>("num_train", d.num_train);
            gd.write_value<std::int64_t>("num_test", d.num_test);
            gd.write_array<std::uint8_t>("train", d.train);
            gd.write_array<std::uint8_t>("test", d.test);
            gd.write_array<std::uint8_t>("expected", d.expected);
        });
    }

    inline setup::Fn build_input(std::string algo, Dataset d) {
        return [algo = std::move(algo), d = std::move(d)](TestData& td) {
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                write_input(v, d);
            });
        };
    }

    inline setup::Fn copy_feature_input(std::string algo,
                                        std::int64_t num_features,
                                        std::int64_t num_train,
                                        std::int64_t num_test,
                                        std::int64_t num_values,
                                        std::uint64_t seed = 0) {
        return build_input(std::move(algo),
            make_copy_feature(num_features, num_train, num_test, num_values, seed));
    }

    inline verify::Fn accuracy_at_least(double min_acc) {
        return [min_acc](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            TestData d = vars.read_object("data");
            auto num_test = d.read_value<std::int64_t>("num_test");
            auto expected = d.read_array<std::uint8_t>("expected");
            auto pred = out.read_array<std::uint8_t>("pred");
            if(static_cast<std::int64_t>(pred.size()) != num_test)
                return {false, "pred size " + std::to_string(pred.size())
                    + " != num_test " + std::to_string(num_test)};
            std::int64_t correct = 0;
            for(std::int64_t i = 0; i < num_test; ++i)
                if(pred[i] == expected[i]) ++correct;
            double acc = static_cast<double>(correct) / num_test;
            if(acc < min_acc)
                return {false,
                    "accuracy " + std::to_string(acc)
                    + " < required " + std::to_string(min_acc)
                    + " (" + std::to_string(correct) + "/"
                    + std::to_string(num_test) + ")"};
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_size(std::int64_t expected_n) {
        return [expected_n](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto a = out.read_array<std::uint8_t>("pred");
            if(static_cast<std::int64_t>(a.size()) != expected_n)
                return {false, "size " + std::to_string(a.size())
                    + " != " + std::to_string(expected_n)};
            return {true, std::string{}};
        };
    }

} // namespace classify_common
