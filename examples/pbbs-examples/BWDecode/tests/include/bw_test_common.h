// Shared helpers for BWDecode scenarios. We embed a tiny sequential BW
// encoder so the test harness can build (plaintext, encoded) pairs
// deterministically - the student gets the encoded string and must
// recover the plaintext. The encoder follows the same convention as
// pbbsbench: append a null sentinel to the plaintext, rotate, sort
// rotations, take the last column.

#pragma once

#include <algorithm>
#include <cstdint>
#include <dataGen.h>
#include <numeric>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <vector>

namespace bw_common {

    // Sequential BWT used only to build test inputs. The plaintext must
    // not contain a null character.
    inline std::string bw_encode(const std::string& plaintext) {
        std::string s = plaintext + '\0';
        std::size_t n = s.size();
        std::vector<std::size_t> rot(n);
        std::iota(rot.begin(), rot.end(), 0);
        std::sort(rot.begin(), rot.end(),
                  [&](std::size_t a, std::size_t b) {
                      for(std::size_t k = 0; k < n; ++k) {
                          unsigned char ca = static_cast<unsigned char>(s[(a + k) % n]);
                          unsigned char cb = static_cast<unsigned char>(s[(b + k) % n]);
                          if(ca != cb) return ca < cb;
                      }
                      return false;
                  });
        std::string out(n, '\0');
        for(std::size_t i = 0; i < n; ++i)
            out[i] = s[(rot[i] + n - 1) % n];
        return out;
    }

    inline setup::Fn fixed_input(std::string algo, std::string plaintext) {
        return [algo = std::move(algo),
                plaintext = std::move(plaintext)](TestData& td) {
            std::string encoded = bw_encode(plaintext);
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_string("encoded", encoded);
                v.write_string("plaintext", plaintext);
            });
        };
    }

    // Random ASCII (printable, no nulls) of given length.
    inline setup::Fn random_input(std::string algo, std::size_t n,
                                  std::uint64_t seed = 0) {
        return [algo = std::move(algo), n, seed](TestData& td) {
            std::string plaintext(n, '\0');
            // Printable ASCII range, dodging the null sentinel.
            constexpr int range = 'z' - 'a' + 1;
            for(std::size_t i = 0; i < n; ++i) {
                plaintext[i] = static_cast<char>(
                    'a' + (dataGen::hash<unsigned int>(seed + i) % range));
            }
            std::string encoded = bw_encode(plaintext);
            td.write_object("vars", [&](TestData& v) {
                v.write_string("algo", algo);
                v.write_string("encoded", encoded);
                v.write_string("plaintext", plaintext);
            });
        };
    }

    // Verify: decoded output equals the original plaintext.
    inline verify::Fn matches_plaintext() {
        return [](const TestData& in, const TestData& out)
                -> std::pair<bool, std::string> {
            TestData vars = in.read_object("vars");
            auto expected = vars.read_string("plaintext");
            auto got = out.read_string("decoded");
            if(got.size() != expected.size())
                return {false,
                    "size: expected " + std::to_string(expected.size())
                    + ", got " + std::to_string(got.size())};
            for(std::size_t i = 0; i < expected.size(); ++i) {
                if(got[i] != expected[i])
                    return {false,
                        "mismatch at i=" + std::to_string(i)
                        + " expected '" + std::string(1, expected[i])
                        + "' got '" + std::string(1, got[i]) + "'"};
            }
            return {true, std::string{}};
        };
    }

    inline verify::Fn has_size(std::size_t expected) {
        return [expected](const TestData&, const TestData& out)
                -> std::pair<bool, std::string> {
            auto s = out.read_string("decoded");
            if(s.size() != expected)
                return {false,
                    "size: expected " + std::to_string(expected)
                    + ", got " + std::to_string(s.size())};
            return {true, std::string{}};
        };
    }

} // namespace bw_common
