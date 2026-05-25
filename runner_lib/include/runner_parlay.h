#pragma once

/**
 * @file runner_parlay.h
 * @brief Parlay-specific input/output helpers for the runner.
 *
 * Standard runner.h gives you read_array<T> -> std::vector<T>. For
 * parlay-targeting wrappers the std::vector -> parlay::sequence conversion
 * (and back) inside the timed RUNNER_EXECUTE region was the dominant
 * source of overhead vs raw pbbs benchmarks - it adds two
 * memory-bandwidth-bound passes per timed iteration.
 *
 * These helpers materialise the parlay::sequence *outside* RUNNER_EXECUTE
 * so the timed region sees only the algorithm body, matching pbbs's
 * `time_loop` semantics. Trade-off: the student function signatures must
 * accept parlay-native types directly - the same wrapper no longer
 * compiles against the OMP / Cilk / sequential runner variants.
 *
 * Only include this header from a parlay-only solution.
 */

#include <cstddef>
#include <string>
#include <vector>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include <runner.h>
#include <test_data.h>


namespace runner {

    /// Read a key from runner::input() as a parlay::sequence. The raw
    /// bytes are dropped from input() after parsing (single-shot), same
    /// semantics as read_array<T>.
    ///
    /// The internal parlay::tabulate runs on the active scheduler so the
    /// resulting pages are NUMA-distributed - the first parallel pass
    /// over `seq` inside RUNNER_EXECUTE no longer pays first-touch.
    template<typename T>
    parlay::sequence<T> read_parlay_sequence(const std::string& k) {
        std::vector<T> v = input().read_array<T>(k);
        input().erase(k);
        const T* src = v.data();
        return parlay::tabulate(
            v.size(),
            [src](std::size_t i) -> T { return src[i]; });
    }

    /// Same, but reads from a nested TestData (e.g. the object returned
    /// by `runner::read_object("vars")`).
    template<typename T>
    parlay::sequence<T> read_parlay_sequence(TestData& vars,
                                             const std::string& k) {
        std::vector<T> v = vars.read_array<T>(k);
        vars.erase(k);
        const T* src = v.data();
        return parlay::tabulate(
            v.size(),
            [src](std::size_t i) -> T { return src[i]; });
    }

    /// Read a TLV string value into a parlay::sequence of `CharT` (char or
    /// unsigned char). The tabulate runs outside RUNNER_EXECUTE so the
    /// per-byte materialisation does not enter the timed region - the
    /// dominant cost for the big-text benchmarks (wordCounts, suffixArray).
    template<typename CharT>
    parlay::sequence<CharT> read_parlay_chars(TestData& vars,
                                              const std::string& k) {
        std::string s = vars.read_string(k);
        vars.erase(k);
        const char* src = s.data();
        return parlay::tabulate(
            s.size(),
            [src](std::size_t i) -> CharT {
                return static_cast<CharT>(static_cast<unsigned char>(src[i]));
            });
    }

    /// Read a key stored as `Stored` (the TLV element type) and narrow it
    /// to `Out` during the materialisation. Used by graph wrappers whose
    /// TLV stores int64 offsets/neighbors but whose native graph width is
    /// 32-bit - the narrowing tabulate runs here, outside RUNNER_EXECUTE,
    /// instead of inside the timed region as it used to.
    template<typename Out, typename Stored>
    parlay::sequence<Out> read_parlay_sequence_narrow(TestData& vars,
                                                      const std::string& k) {
        std::vector<Stored> v = vars.read_array<Stored>(k);
        vars.erase(k);
        const Stored* src = v.data();
        return parlay::tabulate(
            v.size(),
            [src](std::size_t i) -> Out { return static_cast<Out>(src[i]); });
    }

    /// Materialise a parlay::sequence into the TLV output map, narrowing /
    /// converting each element from `Src` to the TLV type `Out`. The
    /// parallel pass is outside RUNNER_EXECUTE.
    template<typename Out, typename Src>
    void write_parlay_sequence_as(const std::string& k,
                                  const parlay::sequence<Src>& s) {
        std::vector<Out> v(s.size());
        Out* dst = v.data();
        parlay::parallel_for(
            std::size_t{0}, s.size(),
            [dst, &s](std::size_t i) { dst[i] = static_cast<Out>(s[i]); });
        output().write_array<Out>(k, v);
    }

    /// Materialise a parlay::sequence into the TLV output map.
    /// The parlay::parallel_for here is outside RUNNER_EXECUTE so it does
    /// not enter the timed region.
    template<typename T>
    void write_parlay_sequence(const std::string& k,
                               const parlay::sequence<T>& s) {
        std::vector<T> v(s.size());
        T* dst = v.data();
        parlay::parallel_for(
            std::size_t{0}, s.size(),
            [dst, &s](std::size_t i) { dst[i] = s[i]; });
        output().write_array<T>(k, v);
    }

} // namespace runner
