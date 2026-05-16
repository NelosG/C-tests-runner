#pragma once

/**
 * @file test_data.h
 * @brief Map-of-typed-values shared between the runner, JSON scenarios, and
 *        teacher-written C++ plugins.
 *
 * On-disk format (single file per side, e.g. input.bin / output.bin):
 *
 *   For each (tag, data) record:
 *     [u64 tag_length]            length of tag in bytes
 *     [tag_length bytes]          tag (utf-8, no NUL terminator)
 *     [u64 data_length]           length of data in bytes
 *     [data_length bytes]         opaque payload - interpreted at access time
 *
 * Records repeat until EOF. Order on disk does not matter; access is by key.
 *
 * Per-type payload encoding:
 *
 *   long long / scalar  : [i64]                       (data_length == 8)
 *   long long / array   : [i64]*N                     (data_length == 8*N)
 *   double / scalar     : [f64]                       (data_length == 8)
 *   double / array      : [f64]*N
 *   bool / scalar       : [u8] (0 or 1)               (data_length == 1)
 *   bool / array        : [u8]*N
 *   std::string scalar  : raw utf-8 bytes             (data_length == byte length of the string)
 *   std::string array   : [u64 count][u64 len_0][bytes_0]...[u64 len_{N-1}][bytes_{N-1}]
 *
 * The reader knows which decoder to apply from the read_*<T> call site. Type
 * mismatches between writer and reader produce wrong values silently - keep
 * the schema between scenario / runner / verify in lockstep.
 */

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

class TestData {
    public:
        // ---- Constructors -----------------------------------------------------

        TestData() = default;

        /// Read a TestData payload from disk. Returns an empty map if the file
        /// doesn't exist (so verify() can detect "runner produced nothing").
        static TestData load(const std::filesystem::path& file) {
            TestData td;
            std::error_code ec;
            if(!std::filesystem::exists(file, ec) || ec) return td;
            std::ifstream f(file, std::ios::binary);
            if(!f) return td;
            td.deserialize(f);
            return td;
        }

        /// Write the entire map to disk in one shot.
        void save(const std::filesystem::path& file) const {
            std::ofstream f(file, std::ios::binary);
            if(!f) throw std::runtime_error("TestData::save: cannot open " + file.string());
            serialize(f);
        }

        // ---- Introspection ----------------------------------------------------

        bool contains(const std::string& key) const { return entries_.count(key) > 0; }
        std::size_t size() const { return entries_.size(); }
        bool empty() const { return entries_.empty(); }

        // ---- Typed writers ----------------------------------------------------

        template<typename T>
        void write_value(const std::string& key, T value) {
            static_assert(std::is_arithmetic_v<T>, "write_value requires arithmetic T");
            if constexpr(std::is_same_v<T, bool>) {
                unsigned char b = value ? 1 : 0;
                write_arithmetic_array<unsigned char>(key, &b, 1);
            } else {
                write_arithmetic_array<T>(key, &value, 1);
            }
        }

        template<typename T>
        void write_array(const std::string& key, const std::vector<T>& data) {
            static_assert(std::is_arithmetic_v<T>, "write_array requires arithmetic T");
            if constexpr(std::is_same_v<T, bool>) {
                // std::vector<bool> is bit-packed; serialise as one byte per element.
                std::vector<unsigned char> u8(data.size());
                for(std::size_t i = 0; i < data.size(); ++i) u8[i] = data[i] ? 1 : 0;
                write_arithmetic_array<unsigned char>(key, u8.data(), u8.size());
            } else {
                write_arithmetic_array<T>(key, data.data(), data.size());
            }
        }

        void write_string(const std::string& key, const std::string& s) {
            std::vector<std::uint8_t> bytes(s.begin(), s.end());
            entries_[key] = std::move(bytes);
        }

        void write_strings(const std::string& key, const std::vector<std::string>& v) {
            std::vector<std::uint8_t> blob;
            std::uint64_t count = v.size();
            append_bytes(blob, &count, sizeof(count));
            for(const auto& s : v) {
                std::uint64_t len = s.size();
                append_bytes(blob, &len, sizeof(len));
                blob.insert(blob.end(), s.begin(), s.end());
            }
            entries_[key] = std::move(blob);
        }

        // ---- Typed readers (non-consuming, const) -----------------------------
        //
        // read_* parses the bytes for `key` into a typed value and returns it.
        // Bytes stay in the map; calling read_* twice for the same key returns
        // the same value again. This keeps verify-side lambdas idiomatic
        // (`[](const TestData& in, const TestData& out) {...}`).
        //
        // Memory note: the runner facade (runner_lib::read_*) follows each read
        // with an explicit erase() so peak memory inside the sandboxed runner
        // doesn't carry the raw bytes alongside the parsed vector. Verify-side
        // teacher code does not need to erase - inputs/outputs there are tiny
        // compared to runner-side data.

        template<typename T>
        T read_value(const std::string& key) const {
            static_assert(std::is_arithmetic_v<T>, "read_value requires arithmetic T");
            if constexpr(std::is_same_v<T, bool>) {
                auto v = view_arithmetic_array<unsigned char>(key);
                if(v.empty())
                    throw std::runtime_error("TestData::read_value: '" + key + "' is empty");
                return v[0] != 0;
            } else {
                auto v = view_arithmetic_array<T>(key);
                if(v.empty())
                    throw std::runtime_error("TestData::read_value: '" + key + "' is empty");
                return v[0];
            }
        }

        template<typename T>
        std::vector<T> read_array(const std::string& key) const {
            static_assert(std::is_arithmetic_v<T>, "read_array requires arithmetic T");
            if constexpr(std::is_same_v<T, bool>) {
                auto u8 = view_arithmetic_array<unsigned char>(key);
                std::vector<bool> out(u8.size());
                for(std::size_t i = 0; i < u8.size(); ++i) out[i] = u8[i] != 0;
                return out;
            } else {
                return view_arithmetic_array<T>(key);
            }
        }

        std::string read_string(const std::string& key) const {
            const auto& blob = view(key);
            return std::string(blob.begin(), blob.end());
        }

        std::vector<std::string> read_strings(const std::string& key) const {
            const auto& blob = view(key);
            std::vector<std::string> out;
            std::size_t off = 0;
            if(blob.size() < sizeof(std::uint64_t))
                throw std::runtime_error("TestData::read_strings: truncated header for '" + key + "'");
            std::uint64_t count = read_le<std::uint64_t>(blob.data() + off);
            off += sizeof(count);
            out.reserve(static_cast<std::size_t>(count));
            for(std::uint64_t i = 0; i < count; ++i) {
                if(blob.size() < off + sizeof(std::uint64_t))
                    throw std::runtime_error("TestData::read_strings: truncated length in '" + key + "'");
                std::uint64_t len = read_le<std::uint64_t>(blob.data() + off);
                off += sizeof(len);
                if(blob.size() < off + len)
                    throw std::runtime_error("TestData::read_strings: truncated data in '" + key + "'");
                out.emplace_back(reinterpret_cast<const char*>(blob.data() + off), len);
                off += len;
            }
            return out;
        }

        // ---- Introspection -----------------------------------------------------

        /// All keys currently in the map.
        std::vector<std::string> keys() const {
            std::vector<std::string> out;
            out.reserve(entries_.size());
            for(const auto& [k, _] : entries_) out.push_back(k);
            return out;
        }

        /// Drop the raw bytes for `key`. Used by runner_lib::read_* wrappers to
        /// free peak memory after parsing - keeps the sandboxed runner from
        /// holding 80 MB of raw input alongside the 80 MB parsed std::vector.
        /// No-op if the key is absent. Returns true iff something was erased.
        bool erase(const std::string& key) { return entries_.erase(key) > 0; }

    private:
        std::map<std::string, std::vector<std::uint8_t>> entries_;

        template<typename U>
        static U read_le(const std::uint8_t* p) {
            U v;
            std::memcpy(&v, p, sizeof(U));
            return v;
        }

        static void append_bytes(std::vector<std::uint8_t>& dst, const void* src, std::size_t n) {
            const auto* p = reinterpret_cast<const std::uint8_t*>(src);
            dst.insert(dst.end(), p, p + n);
        }

        /// Const lookup of the byte payload for `key`. Throws on miss.
        /// Returns a reference into entries_; caller must not retain it past the
        /// next mutating call (write_/erase/clear).
        const std::vector<std::uint8_t>& view(const std::string& key) const {
            auto it = entries_.find(key);
            if(it == entries_.end())
                throw std::runtime_error("TestData: key '" + key + "' not found");
            return it->second;
        }

        template<typename T>
        void write_arithmetic_array(const std::string& key, const T* data, std::size_t count) {
            std::vector<std::uint8_t> blob(count * sizeof(T));
            if(count > 0) std::memcpy(blob.data(), data, count * sizeof(T));
            entries_[key] = std::move(blob);
        }

        template<typename T>
        std::vector<T> view_arithmetic_array(const std::string& key) const {
            const auto& blob = view(key);
            if(blob.size() % sizeof(T) != 0)
                throw std::runtime_error(
                    "TestData: '" + key + "' size not a multiple of sizeof("
                    + std::to_string(sizeof(T)) + ")"
                );
            std::vector<T> out(blob.size() / sizeof(T));
            if(!out.empty()) std::memcpy(out.data(), blob.data(), blob.size());
            return out;
        }

        void serialize(std::ostream& f) const {
            for(const auto& [tag, data] : entries_) {
                std::uint64_t tag_len = tag.size();
                std::uint64_t data_len = data.size();
                f.write(reinterpret_cast<const char*>(&tag_len), sizeof(tag_len));
                f.write(tag.data(), static_cast<std::streamsize>(tag.size()));
                f.write(reinterpret_cast<const char*>(&data_len), sizeof(data_len));
                if(!data.empty()) {
                    f.write(
                        reinterpret_cast<const char*>(data.data()),
                        static_cast<std::streamsize>(data.size())
                    );
                }
            }
        }

        void deserialize(std::istream& f) {
            while(true) {
                std::uint64_t tag_len = 0;
                f.read(reinterpret_cast<char*>(&tag_len), sizeof(tag_len));
                if(f.eof() || !f) break;
                // sanity cap to avoid pathological allocations on corrupt files
                if(tag_len > (1ULL << 20))
                    throw std::runtime_error("TestData::load: implausible tag length");
                std::string tag(static_cast<std::size_t>(tag_len), '\0');
                f.read(tag.data(), static_cast<std::streamsize>(tag_len));
                if(!f) throw std::runtime_error("TestData::load: truncated tag");

                std::uint64_t data_len = 0;
                f.read(reinterpret_cast<char*>(&data_len), sizeof(data_len));
                if(!f) throw std::runtime_error("TestData::load: truncated data length");
                if(data_len > (2ULL << 30))
                    throw std::runtime_error("TestData::load: implausible data length");
                std::vector<std::uint8_t> data(static_cast<std::size_t>(data_len));
                if(data_len > 0) {
                    f.read(
                        reinterpret_cast<char*>(data.data()),
                        static_cast<std::streamsize>(data_len)
                    );
                    if(!f) throw std::runtime_error("TestData::load: truncated payload");
                }
                entries_[std::move(tag)] = std::move(data);
            }
        }
};
