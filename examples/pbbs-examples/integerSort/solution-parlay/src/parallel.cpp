// Single TU - all variants live here (currently just parallel radix sort).
//
// Parlay-native signature: input/output are parlay::sequence directly,
// so there is no std::vector <-> parlay::sequence conversion in the
// timed region. The runner main does the TLV-to-sequence materialisation
// outside RUNNER_EXECUTE via runner::read_parlay_sequence /
// runner::write_parlay_sequence.

#include <parlay/primitives.h>

#include <isort.h>
#include <pbbs_parallel_radix_sort.h>

namespace student {

    parlay::sequence<std::uint32_t> parallel_radix_sort(
        parlay::sequence<std::uint32_t>& in,
        std::size_t bits)
    {
        return pbbs_parallel_radix_sort::int_sort<std::uint32_t>(
            parlay::make_slice(in), bits);
    }

} // namespace student
