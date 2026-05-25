#pragma once

/**
 * @file test_data.h
 * @brief Map-of-typed-values shared between the runner, JSON scenarios, and
 *        teacher-written C++ plugins.
 *
 * ===========================================================================
 * On-disk format (single file per side, e.g. input.bin / output.bin)
 * ===========================================================================
 *
 *   For each (tag, data) record:
 *     [u64 tag_length]            length of tag in bytes
 *     [tag_length bytes]          tag (utf-8, no NUL terminator)
 *     [u64 data_length]           length of data in bytes
 *     [data_length bytes]         opaque payload - interpreted at access time
 *
 * Records repeat until EOF. Order on disk does not matter; access is by key.
 *
 * ===========================================================================
 * Per-key payload grammar
 * ===========================================================================
 *
 *   scalar<T>                          (write_value / read_value)
 *     where T is trivially-copyable, non-pointer
 *     = [T sizeof(T) bytes]            ; bool is encoded as 1 byte (0 or 1)
 *
 *   array<T>                           (write_array / read_array)
 *     = [u64 N][T * N]                 ; bool elements packed as 1 byte each
 *
 *   array<vector<U>>                   (recursive: write_array / read_array)
 *     = [u64 N_outer][array<U> * N_outer]
 *     = [u64 N_outer][[u64 N_inner_0][U * N_inner_0]] [[u64 N_inner_1]...]...
 *
 *   array<vector<vector<U>>>           ; arbitrary nesting depth
 *     = ... one more u64 N prefix per level ...
 *
 *   map<K, V>                          (write_map / read_map)
 *     = [u64 N][K-block of N keys][V-block of N values]
 *     K-block layout depends on K:
 *       - K POD/arithmetic:  [K * N]
 *       - K bool:            [u8 * N]              (0/1)
 *       - K std::string:     [u64 len_0][bytes_0] [u64 len_1][bytes_1] ...
 *     V-block follows the same rules.
 *
 *   string                             (write_string / read_string)
 *     = raw utf-8 bytes                ; total length in TLV data_length
 *
 *   array<string>                      (write_strings / read_strings)
 *     = [u64 N][u64 len_0][bytes_0] [u64 len_1][bytes_1] ...
 *
 *   object                             (write_object / read_object)
 *     = full recursive TLV stream (same envelope as the outer file format).
 *       The blob holds a serialized TestData with its own named sub-entries:
 *
 *       for each (sub_tag, sub_data) inside the object:
 *         [u64 sub_tag_length][sub_tag bytes]
 *         [u64 sub_data_length][sub_data bytes]
 *
 *     Sub-entries can themselves be any of the above kinds, including
 *     nested objects to arbitrary depth.
 *
 * Integer widths are fixed: u64 for sizes/counts, T's underlying type for
 * data. Little-endian assumed (target platforms are x86_64 only).
 *
 * ===========================================================================
 * Type safety
 * ===========================================================================
 *
 * There are no type tags inside the binary. The reader knows what to do from
 * its template argument at the call site. Mismatches between writer and
 * reader produce wrong values silently (or an exception on truncation). Keep
 * the schema between scenario / runner / verify in lockstep.
 */

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace test_data_detail {

    // ------------------------------------------------------------------------
    // Low-level byte helpers
    // ------------------------------------------------------------------------

    inline void append_bytes(std::vector<std::uint8_t>& dst, const void* src, std::size_t n) {
        const auto* p = static_cast<const std::uint8_t*>(src);
        dst.insert(dst.end(), p, p + n);
    }

    template<typename U>
    inline void append_pod(std::vector<std::uint8_t>& dst, const U& v) {
        std::size_t off = dst.size();
        dst.resize(off + sizeof(U));
        std::memcpy(dst.data() + off, &v, sizeof(U));
    }

    template<typename U>
    inline U load_pod(const std::uint8_t*& p, const std::uint8_t* end, const char* ctx) {
        if(p + sizeof(U) > end)
            throw std::runtime_error(std::string("test_data: truncated ") + ctx);
        U v;
        std::memcpy(&v, p, sizeof(U));
        p += sizeof(U);
        return v;
    }

    inline void check_room(const std::uint8_t* p, const std::uint8_t* end, std::size_t n, const char* ctx) {
        if(static_cast<std::size_t>(end - p) < n)
            throw std::runtime_error(std::string("test_data: truncated ") + ctx);
    }

    // ------------------------------------------------------------------------
    // NestedIO<T> - recursive serializer for `array<T>` payloads
    //
    // Leaf case: T is trivially-copyable. Written/read as raw sizeof(T) bytes.
    // Special case for bool: encoded as a single u8 (so std::vector<bool>'s
    // bit-packed storage round-trips cleanly).
    // Recursive case: T = std::vector<U>. Writes [u64 N] followed by per-element
    // NestedIO<U>::write. The recursion terminates at the leaf.
    // ------------------------------------------------------------------------

    template<typename T>
    struct NestedIO {
        static_assert(
            std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>,
            "test_data: NestedIO leaf T must be trivially-copyable, non-pointer "
            "(POD struct, arithmetic, bool, or enum). Use vector<...> for collections."
        );

        static void write(std::vector<std::uint8_t>& blob, const T& x) {
            append_pod<T>(blob, x);
        }

        static T read(const std::uint8_t*& p, const std::uint8_t* end) {
            return load_pod<T>(p, end, "POD leaf");
        }
    };

    template<>
    struct NestedIO<bool> {
        static void write(std::vector<std::uint8_t>& blob, bool x) {
            blob.push_back(x ? std::uint8_t{1} : std::uint8_t{0});
        }
        static bool read(const std::uint8_t*& p, const std::uint8_t* end) {
            check_room(p, end, 1, "bool leaf");
            bool v = (*p != 0);
            p += 1;
            return v;
        }
    };

    template<typename U>
    struct NestedIO<std::vector<U>> {
        static void write(std::vector<std::uint8_t>& blob, const std::vector<U>& v) {
            std::uint64_t n = v.size();
            append_pod<std::uint64_t>(blob, n);
            if constexpr(std::is_same_v<U, bool>) {
                // Bulk-encode std::vector<bool>: one u8 per element.
                std::size_t off = blob.size();
                blob.resize(off + static_cast<std::size_t>(n));
                for(std::uint64_t i = 0; i < n; ++i)
                    blob[off + static_cast<std::size_t>(i)] = v[static_cast<std::size_t>(i)] ? 1 : 0;
            } else if constexpr(std::is_trivially_copyable_v<U> && !std::is_pointer_v<U>) {
                // Bulk memcpy for POD leaf - skips the per-element loop.
                std::size_t bytes = static_cast<std::size_t>(n) * sizeof(U);
                std::size_t off = blob.size();
                blob.resize(off + bytes);
                if(bytes > 0) std::memcpy(blob.data() + off, v.data(), bytes);
            } else {
                for(const auto& x : v) NestedIO<U>::write(blob, x);
            }
        }

        static std::vector<U> read(const std::uint8_t*& p, const std::uint8_t* end) {
            std::uint64_t n = load_pod<std::uint64_t>(p, end, "vector size");
            // Sanity cap: 2^40 elements is well beyond any realistic test
            // input. Reject before allocating to avoid OOM on corrupt files.
            if(n > (1ULL << 40))
                throw std::runtime_error("test_data: implausible vector size");
            std::vector<U> result;
            if constexpr(std::is_same_v<U, bool>) {
                check_room(p, end, static_cast<std::size_t>(n), "vector<bool> data");
                result.resize(static_cast<std::size_t>(n));
                for(std::uint64_t i = 0; i < n; ++i)
                    result[static_cast<std::size_t>(i)] = p[i] != 0;
                p += static_cast<std::size_t>(n);
            } else if constexpr(std::is_trivially_copyable_v<U> && !std::is_pointer_v<U>) {
                std::size_t bytes = static_cast<std::size_t>(n) * sizeof(U);
                check_room(p, end, bytes, "vector<POD> data");
                result.resize(static_cast<std::size_t>(n));
                if(bytes > 0) std::memcpy(result.data(), p, bytes);
                p += bytes;
            } else {
                result.reserve(static_cast<std::size_t>(n));
                for(std::uint64_t i = 0; i < n; ++i)
                    result.push_back(NestedIO<U>::read(p, end));
            }
            return result;
        }
    };

    // ------------------------------------------------------------------------
    // Map field helpers - one element write/read for K-block or V-block
    // ------------------------------------------------------------------------

    inline void write_str_field(std::vector<std::uint8_t>& blob, const std::string& s) {
        std::uint64_t len = s.size();
        append_pod<std::uint64_t>(blob, len);
        if(len > 0) append_bytes(blob, s.data(), s.size());
    }

    inline std::string read_str_field(const std::uint8_t*& p, const std::uint8_t* end) {
        std::uint64_t len = load_pod<std::uint64_t>(p, end, "map string field length");
        if(len > (1ULL << 30))
            throw std::runtime_error("test_data: implausible string length in map field");
        check_room(p, end, static_cast<std::size_t>(len), "map string field data");
        std::string s(reinterpret_cast<const char*>(p), static_cast<std::size_t>(len));
        p += static_cast<std::size_t>(len);
        return s;
    }

    template<typename T>
    inline void write_map_field(std::vector<std::uint8_t>& blob, const T& x) {
        if constexpr(std::is_same_v<T, std::string>) {
            write_str_field(blob, x);
        } else if constexpr(std::is_same_v<T, bool>) {
            blob.push_back(x ? std::uint8_t{1} : std::uint8_t{0});
        } else {
            static_assert(
                std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>,
                "test_data: map K/V must be POD, bool, or std::string"
            );
            append_pod<T>(blob, x);
        }
    }

    template<typename T>
    inline T read_map_field(const std::uint8_t*& p, const std::uint8_t* end) {
        if constexpr(std::is_same_v<T, std::string>) {
            return read_str_field(p, end);
        } else if constexpr(std::is_same_v<T, bool>) {
            check_room(p, end, 1, "map bool field");
            bool v = (*p != 0);
            p += 1;
            return v;
        } else {
            static_assert(
                std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>,
                "test_data: map K/V must be POD, bool, or std::string"
            );
            return load_pod<T>(p, end, "map POD field");
        }
    }

    // ------------------------------------------------------------------------
    // MapIO<Map> - works on any container exposing key_type/mapped_type and
    // emplace(K, V). Covers std::map, std::unordered_map, std::multimap, and
    // std::unordered_multimap, plus any third-party map-like container that
    // follows the same protocol.
    // ------------------------------------------------------------------------

    template<typename Map>
    struct MapIO {
        using K = typename Map::key_type;
        using V = typename Map::mapped_type;

        static void write(std::vector<std::uint8_t>& blob, const Map& m) {
            std::uint64_t n = m.size();
            append_pod<std::uint64_t>(blob, n);
            // K-block, then V-block. Iterating twice keeps the layout simple
            // and lets the reader skip the K-block bulk for POD K (memcpy)
            // without paying for string-length parsing in the middle.
            for(const auto& kv : m) write_map_field<K>(blob, kv.first);
            for(const auto& kv : m) write_map_field<V>(blob, kv.second);
        }

        static Map read(const std::uint8_t*& p, const std::uint8_t* end) {
            std::uint64_t n = load_pod<std::uint64_t>(p, end, "map size");
            if(n > (1ULL << 30))
                throw std::runtime_error("test_data: implausible map size");
            std::vector<K> keys;
            keys.reserve(static_cast<std::size_t>(n));
            for(std::uint64_t i = 0; i < n; ++i)
                keys.push_back(read_map_field<K>(p, end));
            Map result;
            for(std::uint64_t i = 0; i < n; ++i) {
                V v = read_map_field<V>(p, end);
                result.emplace(std::move(keys[static_cast<std::size_t>(i)]), std::move(v));
            }
            return result;
        }
    };

} // namespace test_data_detail


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

        /// Write a single trivially-copyable value under `key`. Format: raw
        /// sizeof(T) bytes (bool is encoded as 1 byte).
        template<typename T>
        void write_value(const std::string& key, T value) {
            static_assert(
                std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>,
                "write_value requires trivially-copyable, non-pointer T"
            );
            std::vector<std::uint8_t> blob;
            test_data_detail::NestedIO<T>::write(blob, value);
            entries_[key] = std::move(blob);
        }

        /// Write an array (1D or arbitrarily nested via std::vector<...> of T).
        /// T can be any trivially-copyable POD, or another std::vector to nest.
        /// Format is the recursive [u64 N][serialize(T) * N] grammar in the
        /// file-level docstring above.
        template<typename T>
        void write_array(const std::string& key, const std::vector<T>& data) {
            std::vector<std::uint8_t> blob;
            test_data_detail::NestedIO<std::vector<T>>::write(blob, data);
            entries_[key] = std::move(blob);
        }

        /// Write a map-like container (std::map, std::unordered_map, multimap
        /// variants, or third-party equivalents). K and V can be POD, bool, or
        /// std::string. Format: [u64 N][K-block][V-block].
        template<typename Map>
        void write_map(const std::string& key, const Map& m) {
            std::vector<std::uint8_t> blob;
            test_data_detail::MapIO<Map>::write(blob, m);
            entries_[key] = std::move(blob);
        }

        /// Write a composite object as a recursive TLV blob. The builder
        /// callback receives a fresh sub-TestData; whatever it writes there
        /// is serialized into the blob using the same TLV envelope as the
        /// outer file format. Compose objects to arbitrary depth.
        ///
        /// Example:
        ///   td.write_object("graph", [&](TestData& g) {
        ///       g.write_value<long long>("numV", n);
        ///       g.write_array<long long>("offsets", offsets);
        ///       g.write_array<long long>("neighbors", neighbors);
        ///   });
        template<typename F>
        void write_object(const std::string& key, F&& builder) {
            TestData sub;
            std::forward<F>(builder)(sub);
            std::ostringstream buf(std::ios::binary);
            sub.serialize(buf);
            const std::string s = buf.str();
            entries_[key] = std::vector<std::uint8_t>(s.begin(), s.end());
        }

        void write_string(const std::string& key, const std::string& s) {
            std::vector<std::uint8_t> bytes(s.begin(), s.end());
            entries_[key] = std::move(bytes);
        }

        void write_strings(const std::string& key, const std::vector<std::string>& v) {
            std::vector<std::uint8_t> blob;
            std::uint64_t count = v.size();
            test_data_detail::append_pod<std::uint64_t>(blob, count);
            for(const auto& s : v) {
                test_data_detail::write_str_field(blob, s);
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
            static_assert(
                std::is_trivially_copyable_v<T> && !std::is_pointer_v<T>,
                "read_value requires trivially-copyable, non-pointer T"
            );
            const auto& blob = view(key);
            const std::uint8_t* p = blob.data();
            const std::uint8_t* end = p + blob.size();
            return test_data_detail::NestedIO<T>::read(p, end);
        }

        template<typename T>
        std::vector<T> read_array(const std::string& key) const {
            const auto& blob = view(key);
            const std::uint8_t* p = blob.data();
            const std::uint8_t* end = p + blob.size();
            return test_data_detail::NestedIO<std::vector<T>>::read(p, end);
        }

        template<typename Map>
        Map read_map(const std::string& key) const {
            const auto& blob = view(key);
            const std::uint8_t* p = blob.data();
            const std::uint8_t* end = p + blob.size();
            return test_data_detail::MapIO<Map>::read(p, end);
        }

        /// Parse the recursive-TLV blob under `key` back into a TestData and
        /// return it by value. Calling read_object on a missing key throws
        /// like the other readers. Calling it on a blob written by anything
        /// other than write_object will typically throw during deserialize
        /// (malformed TLV).
        TestData read_object(const std::string& key) const {
            const auto& blob = view(key);
            TestData sub;
            if(blob.empty()) return sub;
            std::string s(blob.begin(), blob.end());
            std::istringstream buf(s, std::ios::binary);
            sub.deserialize(buf);
            return sub;
        }

        std::string read_string(const std::string& key) const {
            const auto& blob = view(key);
            return std::string(blob.begin(), blob.end());
        }

        std::vector<std::string> read_strings(const std::string& key) const {
            const auto& blob = view(key);
            const std::uint8_t* p = blob.data();
            const std::uint8_t* end = p + blob.size();
            std::uint64_t count = test_data_detail::load_pod<std::uint64_t>(p, end, "strings count");
            if(count > (1ULL << 30))
                throw std::runtime_error("test_data: implausible string count");
            std::vector<std::string> out;
            out.reserve(static_cast<std::size_t>(count));
            for(std::uint64_t i = 0; i < count; ++i)
                out.push_back(test_data_detail::read_str_field(p, end));
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

        /// Const lookup of the byte payload for `key`. Throws on miss.
        /// Returns a reference into entries_; caller must not retain it past the
        /// next mutating call (write_/erase/clear).
        const std::vector<std::uint8_t>& view(const std::string& key) const {
            auto it = entries_.find(key);
            if(it == entries_.end())
                throw std::runtime_error("TestData: key '" + key + "' not found");
            return it->second;
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
