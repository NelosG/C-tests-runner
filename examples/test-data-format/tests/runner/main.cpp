// Runner main for the test-data-format example. Reads a fixed set of typed
// keys from input.bin, writes them back to output.bin unchanged. The verify
// side compares input and output to confirm the binary format round-trips
// every supported shape correctly through the runner subprocess.

#include <format_types.h>
#include <identity.h>
#include <map>
#include <runner.h>
#include <string>
#include <vector>

RUNNER_MAIN {
    // Plain arrays (1D POD)
    auto i64v = runner::read_array<long long>("flat_int");
    auto f64v = runner::read_array<double>("flat_double");
    auto bv = runner::read_array<bool>("flat_bool");

    // POD struct arrays
    auto edges = runner::read_array<Edge>("edges");
    auto pts = runner::read_array<Point2d>("points");

    // Nested arrays
    auto m2d = runner::read_array<std::vector<long long>>("matrix_int");
    auto m3d = runner::read_array<std::vector<std::vector<double>>>("cube_double");

    // Maps
    auto m_id = runner::input().read_map<std::map<long long, double>>("m_int_double");
    runner::input().erase("m_int_double");
    auto m_si = runner::input().read_map<std::map<std::string, long long>>("m_str_int");
    runner::input().erase("m_str_int");

    // Scalar
    auto n = runner::read_value<long long>("scalar_int");

    // String + array of strings
    auto text = runner::read_string("text");
    auto tags = runner::read_strings("tags");

    // Single timed block. Trivial: just touch the student dummy to prove the
    // link path works. The interesting work is the I/O on either side.
    RUNNER_EXECUTE {
        student::noop();
    };

    runner::write_array<long long>("flat_int", i64v);
    runner::write_array<double>("flat_double", f64v);
    runner::write_array<bool>("flat_bool", bv);
    runner::write_array<Edge>("edges", edges);
    runner::write_array<Point2d>("points", pts);
    runner::write_array<std::vector<long long>>("matrix_int", m2d);
    runner::write_array<std::vector<std::vector<double>>>("cube_double", m3d);
    runner::output().write_map("m_int_double", m_id);
    runner::output().write_map("m_str_int", m_si);
    runner::write_value<long long>("scalar_int", n);
    runner::write_string("text", text);
    runner::write_strings("tags", tags);
}
