// Shared helpers for wordCounts scenarios.

#pragma once

#include <cstdint>
#include <dataGen.h>
#include <map>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <vector>

namespace wc_common {

    // Reference tokenizer + counter mirroring pbbsbench's approach:
    // lowercase a-z runs are words, anything else is a separator.
    inline std::map<std::string, std::int64_t> reference_counts(
        const std::string& text)
    {
        std::map<std::string, std::int64_t> out;
        std::string cur;
        auto flush = [&]() {
            if(!cur.empty()) { ++out[cur]; cur.clear(); }
        };
        for(char c : text) {
            unsigned char u = static_cast<unsigned char>(c);
            if(u >= 'A' && u <= 'Z') { cur.push_back(static_cast<char>(u + 32)); }
            else if(u >= 'a' && u <= 'z') { cur.push_back(c); }
            else { flush(); }
        }
        flush();
        return out;
    }

    inline setup::Fn fixed_input(std::string algo, std::string text) {
        return [algo = std::move(algo), text = std::move(text)](TestData& td) {
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_string("text", text);
            });
        };
    }

    // Build a deterministic text of n characters: lowercase letters
    // separated by random short non-letter runs.
    inline setup::Fn random_input(std::string algo, std::size_t n,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), n, seed](TestData& td) {
            std::string text;
            text.reserve(n);
            for(std::size_t i = 0; i < n; ++i) {
                std::uint32_t h = dataGen::hash<unsigned int>(seed + i);
                // 80% letters, 20% spaces - guarantees some tokenization.
                if((h % 5) == 0) text.push_back(' ');
                else text.push_back(static_cast<char>('a' + (h % 26)));
            }
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_string("text", text);
            });
        };
    }

    // Verify: compare student's word->count map to the reference.
    inline verify::Fn matches_reference() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            using MapT = std::map<std::string, std::int64_t>;
            TestData vars = in.read_object("vars");
            auto text = vars.read_string("text");
            MapT got = out.read_map<MapT>("counts");
            MapT expected = reference_counts(text);
            if(got.size() != expected.size())
                return {false,
                    "distinct word count: expected " + std::to_string(expected.size())
                    + ", got " + std::to_string(got.size())};
            for(const auto& [word, cnt] : expected) {
                auto it = got.find(word);
                if(it == got.end())
                    return {false, "missing word '" + word + "'"};
                if(it->second != cnt)
                    return {false,
                        "word '" + word + "' expected " + std::to_string(cnt)
                        + " got " + std::to_string(it->second)};
            }
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_some_words() {
        return [](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            using MapT = std::map<std::string, std::int64_t>;
            MapT m = out.read_map<MapT>("counts");
            if(m.empty()) return {false, "no words found"};
            return {true, std::string{}};
        };
    }

} // namespace wc_common
