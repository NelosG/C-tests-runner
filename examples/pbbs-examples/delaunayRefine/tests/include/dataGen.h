// Vendored from pbbsbench/common/dataGen.h with parlay::hash64 inlined
// (so tests don't need parlay on their include path - they only need it
// transitively via test_engine + runner_lib).
// MIT licensed, (c) Guy Blelloch and the PBBS team.
#ifndef PBBS_DATAGEN_H_
#define PBBS_DATAGEN_H_

#include <cstdint>
#include <cstddef>

namespace dataGen {

    // From parlay/utilities.h - 64-bit hash mixer.
    inline std::uint64_t hash64(std::uint64_t u) {
        std::uint64_t v = u * 3935559000370003845UL + 2691343689449507681UL;
        v ^= v >> 21;
        v ^= v << 37;
        v ^= v >> 4;
        v *= 4768777513237032717UL;
        v ^= v << 20;
        v ^= v >> 41;
        v ^= v << 5;
        return v;
    }

#define HASH_MAX_INT ((unsigned) 1 << 31)

    template <class T> T hash(std::size_t i);

    template <>
    inline int hash<int>(std::size_t i) {
        return hash64(i) & ((((std::size_t) 1) << 31) - 1);
    }

    template <>
    inline long hash<long>(std::size_t i) {
        return hash64(i) & ((((std::size_t) 1) << 63) - 1);
    }

    template <>
    inline unsigned int hash<unsigned int>(std::size_t i) {
        return static_cast<unsigned int>(hash64(i));
    }

    template <>
    inline std::size_t hash<std::size_t>(std::size_t i) {
        return hash64(i);
    }

    template <>
    inline double hash<double>(std::size_t i) {
        return static_cast<double>(hash<int>(i))
             / static_cast<double>((((std::size_t) 1) << 31) - 1);
    }

    template <>
    inline float hash<float>(std::size_t i) {
        return static_cast<float>(
            static_cast<double>(hash<int>(i))
            / static_cast<double>((((std::size_t) 1) << 31) - 1)
        );
    }

} // namespace dataGen

#endif
