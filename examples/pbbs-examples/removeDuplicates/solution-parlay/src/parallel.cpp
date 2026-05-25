// Parlay-native: input/output are parlay::sequence directly, no conversion
// inside the timed region.

#include <dedup.h>
#include <pbbs_parlayhash_dedup.h>

namespace student {

    parlay::sequence<std::uint32_t> parlayhash_dedup(
        const parlay::sequence<std::uint32_t>& in)
    {
        return pbbs_parlayhash_dedup::dedup(in);
    }

} // namespace student
