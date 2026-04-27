#include <scan.h>

#include <parlay/primitives.h>
#include <parlay/sequence.h>


namespace parallel {

    void scan(const std::vector<long long>& array, std::vector<long long>& result) {
        parlay::sequence<long long> input(array.begin(), array.end());

        // parlay::scan is exclusive - convert to inclusive by adding the original.
        auto [exclusive, total] = parlay::scan(input);

        result.resize(array.size());
        parlay::parallel_for(
            0,
            static_cast<long>(array.size()),
            [&](long i) {
                result[i] = exclusive[i] + array[i];
            }
        );
    }

} // namespace parallel
