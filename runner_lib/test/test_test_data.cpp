// Unit tests for TestData - the typed key/value blob shared between the
// sandboxed runner process, JSON scenario loader, and teacher plugins.
//
// The on-disk TLV format is described in test_data.h; we exercise the
// round-trip + boundary behaviour through the public API.

#include <cstdint>
#include <fstream>
#include <gtest/gtest.h>
#include <map>
#include <stdexcept>
#include <test_data.h>
#include <unordered_map>
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

// -----------------------------------------------------------------------------
// POD-struct arrays (trivially-copyable user types)
// -----------------------------------------------------------------------------

namespace {
    struct Edge {
        std::int32_t u;
        std::int32_t v;
        float w;
        bool operator==(const Edge& o) const { return u == o.u && v == o.v && w == o.w; }
    };
    static_assert(std::is_trivially_copyable_v<Edge>);

    struct Point2d {
        double x;
        double y;
        bool operator==(const Point2d& o) const { return x == o.x && y == o.y; }
    };
    static_assert(std::is_trivially_copyable_v<Point2d>);
}

TEST(TestData, pod_struct_round_trip_as_array) {
    TestData td;
    std::vector<Edge> in{{0, 1, 1.5f}, {2, 3, 2.5f}, {4, 5, -3.25f}};
    td.write_array<Edge>("edges", in);
    auto out = td.read_array<Edge>("edges");
    EXPECT_EQ(out, in);
}

TEST(TestData, pod_struct_round_trip_as_scalar) {
    TestData td;
    Edge e{7, 11, 0.5f};
    td.write_value<Edge>("e", e);
    EXPECT_EQ(td.read_value<Edge>("e"), e);
}

// -----------------------------------------------------------------------------
// Nested arrays - arbitrary depth via recursive NestedIO
// -----------------------------------------------------------------------------

TEST(TestData, nested_2d_int_round_trip) {
    TestData td;
    std::vector<std::vector<long long>> in{
        {1, 2, 3},
        {4, 5},
        {},
        {6, 7, 8, 9}
    };
    td.write_array<std::vector<long long>>("m", in);
    EXPECT_EQ(td.read_array<std::vector<long long>>("m"), in);
}

TEST(TestData, nested_3d_double_round_trip) {
    TestData td;
    std::vector<std::vector<std::vector<double>>> cube{
        {{1.0, 2.0}, {3.0}},
        {{}, {4.5, 5.5, 6.5}},
        {}
    };
    td.write_array<std::vector<std::vector<double>>>("cube", cube);
    EXPECT_EQ(td.read_array<std::vector<std::vector<double>>>("cube"), cube);
}

TEST(TestData, nested_pod_struct_round_trip) {
    TestData td;
    std::vector<std::vector<Point2d>> rows{
        {{1.0, 2.0}, {3.0, 4.0}},
        {},
        {{-1.5, -2.5}}
    };
    td.write_array<std::vector<Point2d>>("rows", rows);
    EXPECT_EQ(td.read_array<std::vector<Point2d>>("rows"), rows);
}

TEST(TestData, empty_nested_arrays) {
    TestData td;
    std::vector<std::vector<long long>> in{};
    td.write_array<std::vector<long long>>("m", in);
    EXPECT_TRUE(td.read_array<std::vector<long long>>("m").empty());
}

// -----------------------------------------------------------------------------
// Maps - K/V combinations covering POD, bool, std::string
// -----------------------------------------------------------------------------

TEST(TestData, map_int_double_round_trip_std_map) {
    TestData td;
    std::map<long long, double> in{{1, 1.5}, {3, -2.0}, {-7, 1e-9}};
    td.write_map("m", in);
    auto got = td.read_map<std::map<long long, double>>("m");
    EXPECT_EQ(got, in);
}

TEST(TestData, map_int_double_round_trip_unordered_map) {
    TestData td;
    std::unordered_map<long long, double> in{{1, 1.5}, {3, -2.0}, {-7, 1e-9}};
    td.write_map("m", in);
    auto out = td.read_map<std::unordered_map<long long, double>>("m");
    EXPECT_EQ(out.size(), in.size());
    for(const auto& [k, v] : in) {
        ASSERT_TRUE(out.count(k));
        EXPECT_DOUBLE_EQ(out.at(k), v);
    }
}

TEST(TestData, map_cross_container_compatibility) {
    // Writing as std::map, reading as std::unordered_map (and vice versa) must
    // both work - the on-disk format only depends on K, V, and N.
    TestData td;
    std::map<long long, long long> in{{1, 100}, {2, 200}, {3, 300}};
    td.write_map("m", in);
    auto unordered = td.read_map<std::unordered_map<long long, long long>>("m");
    EXPECT_EQ(unordered.size(), 3u);
    EXPECT_EQ(unordered.at(1), 100);
    EXPECT_EQ(unordered.at(2), 200);
    EXPECT_EQ(unordered.at(3), 300);
}

TEST(TestData, map_string_int_round_trip) {
    using MapT = std::map<std::string, long long>;
    TestData td;
    MapT in{
        {"alpha", 1},
        {"beta", 2},
        {"", 42},
        {"with spaces and \xC2\xA9 utf-8", 7}
    };
    td.write_map("counts", in);
    auto got = td.read_map<MapT>("counts");
    EXPECT_EQ(got, in);
}

TEST(TestData, map_int_string_round_trip) {
    using MapT = std::map<long long, std::string>;
    TestData td;
    MapT in{
        {0, "zero"},
        {1, ""},
        {42, "the answer"}
    };
    td.write_map("labels", in);
    auto got = td.read_map<MapT>("labels");
    EXPECT_EQ(got, in);
}

TEST(TestData, map_string_string_round_trip) {
    using MapT = std::map<std::string, std::string>;
    TestData td;
    MapT in{
        {"k1", "v1"},
        {"k2", ""},
        {"", "empty key"},
        {"long " "key", "long value with content"}
    };
    td.write_map("kv", in);
    auto got = td.read_map<MapT>("kv");
    EXPECT_EQ(got, in);
}

TEST(TestData, map_bool_bool_round_trip) {
    using MapT = std::map<bool, bool>;
    TestData td;
    MapT in{{true, false}, {false, true}};
    td.write_map("bb", in);
    auto got = td.read_map<MapT>("bb");
    EXPECT_EQ(got, in);
}

TEST(TestData, map_empty_round_trip) {
    using MapT = std::map<long long, long long>;
    TestData td;
    MapT in{};
    td.write_map("e", in);
    auto got = td.read_map<MapT>("e");
    EXPECT_TRUE(got.empty());
}

// -----------------------------------------------------------------------------
// Full on-disk round-trip with the new features
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Objects - recursive TLV blobs holding a sub-TestData
// -----------------------------------------------------------------------------

TEST(TestData, object_with_scalars_and_arrays_round_trip) {
    TestData td;
    td.write_object("graph", [](TestData& g) {
        g.write_value<long long>("numV", 100);
        g.write_array<long long>("offsets", {0, 3, 5});
        g.write_array<long long>("neighbors", {1, 2, 3, 0, 2});
    });

    TestData g = td.read_object("graph");
    EXPECT_EQ(g.read_value<long long>("numV"), 100);
    EXPECT_EQ(
        g.read_array<long long>("offsets"),
        (std::vector<long long>{0, 3, 5})
    );
    EXPECT_EQ(
        g.read_array<long long>("neighbors"),
        (std::vector<long long>{1, 2, 3, 0, 2})
    );
}

TEST(TestData, object_with_string_and_map) {
    using MapT = std::map<std::string, long long>;
    TestData td;
    td.write_object("doc", [](TestData& d) {
        d.write_string("title", "hello world");
        d.write_map("counts", MapT{{"a", 1}, {"b", 2}});
    });

    TestData d = td.read_object("doc");
    EXPECT_EQ(d.read_string("title"), "hello world");
    auto m = d.read_map<MapT>("counts");
    EXPECT_EQ(m, (MapT{{"a", 1}, {"b", 2}}));
}

TEST(TestData, nested_objects_round_trip) {
    TestData td;
    td.write_object("scene", [](TestData& s) {
        s.write_value<long long>("seed", 42);
        s.write_object("graph", [](TestData& g) {
            g.write_value<long long>("numV", 50);
            g.write_array<long long>("offsets", {0, 2, 4});
        });
        s.write_object("points", [](TestData& p) {
            p.write_array<double>("xs", {1.0, 2.0, 3.0});
            p.write_array<double>("ys", {4.0, 5.0, 6.0});
        });
    });

    TestData s = td.read_object("scene");
    EXPECT_EQ(s.read_value<long long>("seed"), 42);

    TestData g = s.read_object("graph");
    EXPECT_EQ(g.read_value<long long>("numV"), 50);
    EXPECT_EQ(g.read_array<long long>("offsets"), (std::vector<long long>{0, 2, 4}));

    TestData p = s.read_object("points");
    EXPECT_EQ(p.read_array<double>("xs"), (std::vector<double>{1.0, 2.0, 3.0}));
    EXPECT_EQ(p.read_array<double>("ys"), (std::vector<double>{4.0, 5.0, 6.0}));
}

TEST(TestData, empty_object_round_trip) {
    TestData td;
    td.write_object("empty", [](TestData&) {});
    TestData e = td.read_object("empty");
    EXPECT_TRUE(e.empty());
}

TEST(TestData, object_survives_disk_round_trip) {
    TempDir dir;
    auto file = dir.path() / "object.bin";

    TestData src;
    src.write_object("graph", [](TestData& g) {
        g.write_value<long long>("numV", 1000);
        g.write_array<std::vector<long long>>("matrix", {{1, 2}, {3, 4, 5}});
    });
    src.save(file);

    TestData loaded = TestData::load(file);
    TestData g = loaded.read_object("graph");
    EXPECT_EQ(g.read_value<long long>("numV"), 1000);
    auto m = g.read_array<std::vector<long long>>("matrix");
    EXPECT_EQ(m, (std::vector<std::vector<long long>>{{1, 2}, {3, 4, 5}}));
}

TEST(TestData, object_can_be_overwritten) {
    TestData td;
    td.write_object("o", [](TestData& s) { s.write_value<long long>("v", 1); });
    td.write_object("o", [](TestData& s) { s.write_value<long long>("v", 999); });
    TestData o = td.read_object("o");
    EXPECT_EQ(o.read_value<long long>("v"), 999);
}

TEST(TestData, read_object_on_garbage_blob_throws) {
    // Writing as scalar then reading as object should throw - the bytes are
    // not a valid TLV stream.
    TestData td;
    td.write_value<long long>("not_an_object", 42);
    EXPECT_THROW(td.read_object("not_an_object"), std::runtime_error);
}

TEST(TestData, save_load_round_trip_nested_and_map) {
    using MatrixT = std::vector<std::vector<long long>>;
    using CountsT = std::map<std::string, long long>;
    TempDir dir;
    auto file = dir.path() / "nested.bin";

    MatrixT matrix{{1, 2}, {3, 4, 5}};
    std::vector<Point2d> points{{1.0, 2.0}, {3.0, 4.0}};
    CountsT counts{{"a", 1}, {"b", 2}};

    TestData src;
    src.write_array<std::vector<long long>>("matrix", matrix);
    src.write_array<Point2d>("points", points);
    src.write_map<CountsT>("counts", counts);
    src.save(file);

    TestData dst = TestData::load(file);
    auto m = dst.read_array<std::vector<long long>>("matrix");
    EXPECT_EQ(m, matrix);
    auto p = dst.read_array<Point2d>("points");
    EXPECT_EQ(p, points);
    auto c = dst.read_map<CountsT>("counts");
    EXPECT_EQ(c, counts);
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
