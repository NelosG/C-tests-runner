// Shared helpers for invertedIndex scenarios.
// We provide a reference build_index that uses std primitives and
// compare the student's formatted-output string against it
// canonicalized.

#pragma once

#include <algorithm>
#include <cstdint>
#include <dataGen.h>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <vector>

namespace index_common {

    // Parse the formatted output "word d0 d1 ... dk\n..." into
    // word -> sorted-unique-doc-ids. The doc id list per word in pbbs
    // output is in group_by_key order; we sort here for the comparison.
    inline std::map<std::string, std::vector<std::int64_t>>
    parse_index(const std::string& formatted)
    {
        std::map<std::string, std::vector<std::int64_t>> out;
        std::size_t i = 0;
        while(i < formatted.size()) {
            std::size_t nl = formatted.find('\n', i);
            std::string line = formatted.substr(
                i, (nl == std::string::npos ? formatted.size() : nl) - i);
            i = (nl == std::string::npos) ? formatted.size() : nl + 1;
            if(line.empty()) continue;
            std::istringstream iss(line);
            std::string word;
            iss >> word;
            std::vector<std::int64_t> ids;
            std::int64_t v;
            while(iss >> v) ids.push_back(v);
            std::sort(ids.begin(), ids.end());
            ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
            out[word] = std::move(ids);
        }
        return out;
    }

    // Sequential reference: split `text` on every occurrence of
    // `doc_start`; in each piece, lower-case + a-z tokens; map word ->
    // {doc_ids it appears in}.
    inline std::map<std::string, std::vector<std::int64_t>>
    reference_index(const std::string& text, const std::string& doc_start)
    {
        std::vector<std::int64_t> starts;
        std::size_t pos = 0;
        while((pos = text.find(doc_start, pos)) != std::string::npos) {
            starts.push_back(static_cast<std::int64_t>(pos));
            pos += doc_start.size();
        }
        std::map<std::string, std::set<std::int64_t>> idx;
        for(std::size_t d = 0; d < starts.size(); ++d) {
            std::size_t start = starts[d] + doc_start.size();
            std::size_t end = (d + 1 == starts.size())
                ? text.size()
                : static_cast<std::size_t>(starts[d + 1]);
            std::string cur;
            auto flush = [&]() {
                if(!cur.empty()) {
                    idx[cur].insert(static_cast<std::int64_t>(d));
                    cur.clear();
                }
            };
            for(std::size_t i = start; i < end; ++i) {
                unsigned char u = static_cast<unsigned char>(text[i]);
                if(u >= 'A' && u <= 'Z') cur.push_back(static_cast<char>(u + 32));
                else if(u >= 'a' && u <= 'z') cur.push_back(text[i]);
                else flush();
            }
            flush();
        }
        std::map<std::string, std::vector<std::int64_t>> out;
        for(auto& [w, s] : idx) out[w] = std::vector<std::int64_t>(s.begin(), s.end());
        return out;
    }

    inline setup::Fn fixed_input(std::string algo, std::string text,
                                 std::string doc_start) {
        return [algo = std::move(algo), text = std::move(text),
                doc_start = std::move(doc_start)](TestData& td) {
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_string("text", text);
                v.write_string("doc_start", doc_start);
            });
        };
    }

    // Build deterministic text: num_docs documents separated by "\n",
    // each doc is some words. doc_start = "\n".
    inline setup::Fn random_input(std::string algo, std::size_t num_docs,
                                  std::size_t words_per_doc,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), num_docs, words_per_doc, seed](TestData& td) {
            std::string text;
            for(std::size_t d = 0; d < num_docs; ++d) {
                text.push_back('\n');
                for(std::size_t w = 0; w < words_per_doc; ++w) {
                    if(w > 0) text.push_back(' ');
                    std::size_t wl = 3 + (dataGen::hash<unsigned int>(
                        seed + d * 1000 + w * 7) % 5);
                    for(std::size_t k = 0; k < wl; ++k) {
                        text.push_back(static_cast<char>(
                            'a' + (dataGen::hash<unsigned int>(
                                seed + d * 7919 + w * 31 + k * 5) % 26)));
                    }
                }
            }
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_string("text", text);
                v.write_string("doc_start", std::string("\n"));
            });
        };
    }

    inline verify::Fn matches_reference() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            auto text = vars.read_string("text");
            auto doc_start = vars.read_string("doc_start");
            auto got_text = out.read_string("formatted");
            auto got = parse_index(got_text);
            auto expected = reference_index(text, doc_start);
            if(got.size() != expected.size())
                return {false,
                    "distinct words: expected " + std::to_string(expected.size())
                    + ", got " + std::to_string(got.size())};
            for(const auto& [w, ids] : expected) {
                auto it = got.find(w);
                if(it == got.end())
                    return {false, "missing word '" + w + "'"};
                if(it->second != ids)
                    return {false, "doc ids for '" + w + "' differ"};
            }
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_some_output() {
        return [](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto s = out.read_string("formatted");
            if(s.empty()) return {false, "empty index"};
            return {true, std::string{}};
        };
    }

} // namespace index_common
