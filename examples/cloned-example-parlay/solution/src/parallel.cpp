#include <scan.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>


namespace parallel {

    void scan(const std::vector<long long>& array, std::vector<long long>& result) {
        // Convert to parlay::sequence
        parlay::sequence<long long> input(array.begin(), array.end());

        // Inclusive scan via parlay: exclusive scan + shift
        auto [exclusive, total] = parlay::scan(input);

        // Convert exclusive scan to inclusive: result[i] = exclusive[i] + input[i]
        result.resize(array.size());
        parlay::parallel_for(
            0,
            static_cast<long>(array.size()),
            [&](long i) {
                result[i] = exclusive[i] + array[i];
            }
        );
    }

}
