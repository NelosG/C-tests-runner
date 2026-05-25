// Verifies that every TestData payload kind survives the full setup ->
// serialize -> runner read -> runner write -> deserialize -> verify cycle
// unchanged. The runner side just echoes inputs to outputs; the verify
// lambda walks each key and compares for equality.

#include <format_types.h>
#include <map>
#include <string>
#include <test_builder.h>
#include <test_data.h>
#include <vector>

namespace {

    void seed(TestData& in) {
        in.write_array<long long>("flat_int", {1, -2, 3, 7, 11});
        in.write_array<double>("flat_double", {1.5, -2.0, 1e-9, 0.0});
        in.write_array<bool>("flat_bool", {true, false, true, true, false});

        in.write_array<Edge>("edges", {
            {0, 1, 1.5f},
            {2, 3, -0.5f},
            {7, 11, 42.25f}
        });
        in.write_array<Point2d>("points", {{1.0, 2.0}, {3.0, 4.0}, {-5.5, 0.0}});

        in.write_array<std::vector<long long>>("matrix_int", {
            {1, 2, 3},
            {4, 5},
            {},
            {6, 7, 8, 9}
        });
        in.write_array<std::vector<std::vector<double>>>("cube_double", {
            {{1.0, 2.0}, {3.0}},
            {{}, {4.5, 5.5, 6.5}},
            {}
        });

        std::map<long long, double> m_id{{1, 1.5}, {3, -2.0}, {-7, 1e-9}};
        in.write_map("m_int_double", m_id);

        std::map<std::string, long long> m_si{
            {"alpha", 1},
            {"beta", 2},
            {"", 42},
            {"with spaces", 7}
        };
        in.write_map("m_str_int", m_si);

        in.write_value<long long>("scalar_int", 12345);
        in.write_string("text", "hello world utf-8 \xC2\xA9");
        in.write_strings("tags", {"a", "", "long tag with spaces", "z"});
    }

    template<typename T>
    std::pair<bool, std::string> check_array(
        const TestData& in,
        const TestData& out,
        const std::string& key
    ) {
        auto e = in.read_array<T>(key);
        auto a = out.read_array<T>(key);
        if(a == e) return {true, {}};
        return {false, "Key '" + key + "': array round-trip mismatch"};
    }

    template<typename Map>
    std::pair<bool, std::string> check_map(
        const TestData& in,
        const TestData& out,
        const std::string& key
    ) {
        auto e = in.read_map<Map>(key);
        auto a = out.read_map<Map>(key);
        if(a == e) return {true, {}};
        return {false, "Key '" + key + "': map round-trip mismatch"};
    }

    std::pair<bool, std::string> verify_all(const TestData& in, const TestData& out) {
        std::pair<bool, std::string> r;

        r = check_array<long long>(in, out, "flat_int"); if(!r.first) return r;
        r = check_array<double>(in, out, "flat_double"); if(!r.first) return r;
        r = check_array<bool>(in, out, "flat_bool"); if(!r.first) return r;
        r = check_array<Edge>(in, out, "edges"); if(!r.first) return r;
        r = check_array<Point2d>(in, out, "points"); if(!r.first) return r;
        r = check_array<std::vector<long long>>(in, out, "matrix_int"); if(!r.first) return r;
        r = check_array<std::vector<std::vector<double>>>(in, out, "cube_double"); if(!r.first) return r;

        using MapID = std::map<long long, double>;
        using MapSI = std::map<std::string, long long>;
        r = check_map<MapID>(in, out, "m_int_double"); if(!r.first) return r;
        r = check_map<MapSI>(in, out, "m_str_int"); if(!r.first) return r;

        if(in.read_value<long long>("scalar_int") != out.read_value<long long>("scalar_int"))
            return {false, "scalar_int mismatch"};
        if(in.read_string("text") != out.read_string("text"))
            return {false, "text mismatch"};
        if(in.read_strings("tags") != out.read_strings("tags"))
            return {false, "tags mismatch"};

        return {true, {}};
    }

} // anonymous namespace

class FormatRoundtripTest final : public TestScenarioExtension {
    public:
        std::vector<Test> get_tests() const override {
            return {
                Test{
                    "all_kinds",
                    [](TestData& in) { seed(in); },
                    verify_all
                }
            };
        }

        std::string name() const override { return "Correctness.FormatRoundtrip"; }
        ScenarioType scenario_type() const override { return ScenarioType::CORRECTNESS; }
};

REGISTER_TEST(FormatRoundtripTest)
