// Unit tests for TestData - the typed key/value blob shared between the
// sandboxed runner process, JSON scenario loader, and teacher plugins.
//
// The on-disk TLV format is described in test_data.h; we exercise the
// round-trip + boundary behaviour through the public API.

#include <cstdint>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <test_data.h>
#include <vector>

#include "test_temp_dir.h"

// -----------------------------------------------------------------------------
// In-memory typed accessors
// (Fresh-map-is-empty is implied by the default-constructed std::map - skipped.)
// -----------------------------------------------------------------------------

TEST(TestData, write_then_read_int_scalar) {
    TestData td;
    td.write_value<long long>("n", 42);
    EXPECT_TRUE(td.contains("n"));
    EXPECT_EQ(td.read_value<long long>("n"), 42);
}

TEST(TestData, write_then_read_double_scalar) {
    TestData td;
    td.write_value<double>("x", 3.5);
    EXPECT_DOUBLE_EQ(td.read_value<double>("x"), 3.5);
}

TEST(TestData, write_then_read_bool_scalar) {
    TestData td;
    td.write_value<bool>("a", true);
    td.write_value<bool>("b", false);
    EXPECT_TRUE(td.read_value<bool>("a"));
    EXPECT_FALSE(td.read_value<bool>("b"));
}

TEST(TestData, write_then_read_int_array) {
    TestData td;
    td.write_array<long long>("v", {1, 2, 3, 4});
    EXPECT_EQ(
        td.read_array<long long>("v"),
        (std::vector<long long>{1, 2, 3, 4})
    );
}

TEST(TestData, write_then_read_empty_array) {
    TestData td;
    td.write_array<long long>("e", {});
    EXPECT_TRUE(td.read_array<long long>("e").empty());
}

TEST(TestData, write_then_read_double_array) {
    TestData td;
    td.write_array<double>("v", {1.5, -2.0, 1e-9});
    auto got = td.read_array<double>("v");
    ASSERT_EQ(got.size(), 3u);
    EXPECT_DOUBLE_EQ(got[0], 1.5);
    EXPECT_DOUBLE_EQ(got[1], -2.0);
    EXPECT_DOUBLE_EQ(got[2], 1e-9);
}

TEST(TestData, write_then_read_bool_array_round_trip) {
    TestData td;
    std::vector<bool> in{true, false, true, true, false};
    td.write_array<bool>("flags", in);
    auto out = td.read_array<bool>("flags");
    ASSERT_EQ(out.size(), in.size());
    for(std::size_t i = 0; i < in.size(); ++i)
        EXPECT_EQ(out[i], in[i]) << i;
}

TEST(TestData, write_then_read_string_scalar) {
    TestData td;
    td.write_string("msg", "hello world");
    EXPECT_EQ(td.read_string("msg"), "hello world");
}

TEST(TestData, write_then_read_empty_string) {
    TestData td;
    td.write_string("empty", "");
    EXPECT_EQ(td.read_string("empty"), "");
    EXPECT_TRUE(td.contains("empty"));
}

TEST(TestData, write_then_read_strings_array) {
    TestData td;
    std::vector<std::string> in{"a", "", "long string ...", "c"};
    td.write_strings("xs", in);
    EXPECT_EQ(td.read_strings("xs"), in);
}

TEST(TestData, read_value_twice_is_idempotent) {
    TestData td;
    td.write_value<long long>("n", 7);
    // read_* is documented non-consuming - must return the same value twice.
    EXPECT_EQ(td.read_value<long long>("n"), 7);
    EXPECT_EQ(td.read_value<long long>("n"), 7);
}

TEST(TestData, write_overwrites_existing_key) {
    TestData td;
    td.write_value<long long>("n", 1);
    td.write_value<long long>("n", 999);
    EXPECT_EQ(td.read_value<long long>("n"), 999);
    EXPECT_EQ(td.size(), 1u);
}

TEST(TestData, erase_removes_key) {
    TestData td;
    td.write_value<long long>("n", 1);
    EXPECT_TRUE(td.erase("n"));
    EXPECT_FALSE(td.contains("n"));
    EXPECT_FALSE(td.erase("n")) << "second erase on the same key should report nothing removed";
    EXPECT_FALSE(td.erase("never-there"));
}

TEST(TestData, keys_lists_every_inserted_key) {
    TestData td;
    td.write_value<long long>("a", 1);
    td.write_value<long long>("b", 2);
    td.write_string("c", "x");
    auto keys = td.keys();
    std::sort(keys.begin(), keys.end());
    EXPECT_EQ(keys, (std::vector<std::string>{"a", "b", "c"}));
}

// -----------------------------------------------------------------------------
// Error handling
// -----------------------------------------------------------------------------

TEST(TestData, read_missing_key_throws) {
    TestData td;
    EXPECT_THROW(td.read_value<long long>("nope"), std::runtime_error);
    EXPECT_THROW(td.read_array<long long>("nope"), std::runtime_error);
    EXPECT_THROW(td.read_string("nope"), std::runtime_error);
    EXPECT_THROW(td.read_strings("nope"), std::runtime_error);
}

TEST(TestData, read_size_mismatch_throws) {
    // Write a 1-byte (bool) blob then try to read it as long long -> size not
    // divisible by 8, must throw rather than silently truncating.
    TestData td;
    td.write_value<bool>("flag", true);
    EXPECT_THROW(td.read_array<long long>("flag"), std::runtime_error);
}

// -----------------------------------------------------------------------------
// save / load - full on-disk round-trip
// -----------------------------------------------------------------------------

TEST(TestData, save_load_round_trip_all_types) {
    TempDir dir;
    auto file = dir.path() / "data.bin";

    TestData src;
    src.write_value<long long>("n", 123);
    src.write_value<double>("x", 1.25);
    src.write_value<bool>("flag", true);
    src.write_array<long long>("nums", {10, 20, 30});
    src.write_array<double>("xs", {0.5, 0.25});
    src.write_array<bool>("bits", {true, false, true});
    src.write_string("msg", "hi");
    src.write_strings("tags", {"a", "bb", ""});
    src.save(file);

    TestData dst = TestData::load(file);
    EXPECT_EQ(dst.read_value<long long>("n"), 123);
    EXPECT_DOUBLE_EQ(dst.read_value<double>("x"), 1.25);
    EXPECT_TRUE(dst.read_value<bool>("flag"));
    EXPECT_EQ(
        dst.read_array<long long>("nums"),
        (std::vector<long long>{10, 20, 30})
    );

    auto bits = dst.read_array<bool>("bits");
    ASSERT_EQ(bits.size(), 3u);
    EXPECT_TRUE(bits[0]);
    EXPECT_FALSE(bits[1]);
    EXPECT_TRUE(bits[2]);

    EXPECT_EQ(dst.read_string ("msg"), "hi");
    EXPECT_EQ(dst.read_strings("tags"), (std::vector<std::string>{"a", "bb", ""}));
}

TEST(TestData, load_missing_file_returns_empty_no_throw) {
    // Documented contract: missing file -> empty map (verify() decides what
    // an empty runner-side output means).
    auto td = TestData::load("definitely/missing/file.bin");
    EXPECT_TRUE(td.empty());
}

TEST(TestData, load_empty_file_returns_empty_map) {
    TempDir dir;
    auto file = dir.path() / "empty.bin";
    {
        std::ofstream f(file, std::ios::binary);
    }  // create empty
    auto td = TestData::load(file);
    EXPECT_TRUE(td.empty());
}

TEST(TestData, save_creates_a_file_even_for_empty_map) {
    TempDir dir;
    auto file = dir.path() / "out.bin";
    TestData td;
    td.save(file);
    EXPECT_TRUE(std::filesystem::exists(file));
}

// -----------------------------------------------------------------------------
// Defensive parsing - truncated / pathological input
// -----------------------------------------------------------------------------

TEST(TestData, load_throws_on_truncated_tag_payload) {
    // Valid 8-byte tag_len header claiming 16 bytes, but only 4 bytes follow -
    // deserialize() must reject the truncated tag rather than fabricate one.
    TempDir dir;
    auto file = dir.path() / "trunc_tag.bin";
    {
        std::ofstream f(file, std::ios::binary);
        std::uint64_t tag_len = 16;
        f.write(reinterpret_cast<const char*>(&tag_len), sizeof(tag_len));
        const char partial_tag[4] = {'a', 'b', 'c', 'd'};
        f.write(partial_tag, sizeof(partial_tag));
    }
    EXPECT_THROW(TestData::load(file), std::runtime_error);
}

TEST(TestData, load_throws_on_truncated_data_payload) {
    // Full tag, valid data_len, but the data bytes are short of what was promised.
    TempDir dir;
    auto file = dir.path() / "trunc_data.bin";
    {
        std::ofstream f(file, std::ios::binary);
        std::uint64_t tag_len = 1;
        char tag = 'k';
        std::uint64_t data_len = 100;
        const char partial_data[10] = {0};
        f.write(reinterpret_cast<const char*>(&tag_len), sizeof(tag_len));
        f.write(&tag, 1);
        f.write(reinterpret_cast<const char*>(&data_len), sizeof(data_len));
        f.write(partial_data, sizeof(partial_data));
    }
    EXPECT_THROW(TestData::load(file), std::runtime_error);
}

TEST(TestData, load_throws_on_implausible_tag_length) {
    TempDir dir;
    auto file = dir.path() / "huge_tag.bin";
    {
        std::ofstream f(file, std::ios::binary);
        // tag_len = 2^21 - above the 1MB sanity cap -> must reject.
        std::uint64_t bogus = (1ULL << 21);
        f.write(reinterpret_cast<const char*>(&bogus), sizeof(bogus));
    }
    EXPECT_THROW(TestData::load(file), std::runtime_error);
}

TEST(TestData, load_throws_on_implausible_data_length) {
    TempDir dir;
    auto file = dir.path() / "huge_data.bin";
    {
        std::ofstream f(file, std::ios::binary);
        // Valid 1-byte tag, then data_len = (3 << 30) above the 2 GiB cap.
        std::uint64_t tag_len = 1;
        char tag = 'k';
        std::uint64_t data_len = (3ULL << 30);
        f.write(reinterpret_cast<const char*>(&tag_len), sizeof(tag_len));
        f.write(&tag, 1);
        f.write(reinterpret_cast<const char*>(&data_len), sizeof(data_len));
    }
    EXPECT_THROW(TestData::load(file), std::runtime_error);
}
