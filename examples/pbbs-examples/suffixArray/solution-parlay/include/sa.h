// Student API for suffixArray. One parallel variant: pbbsbench's
// parallelKS (Karkkainen-Sanders linear-work suffix array).
//
// Parlay-native, 3-phase like pbbs's SATime.C: the character sequence is
// materialised by the runner outside the timed region, only suffixArray() is
// timed (SAContext::run), and the output indices are filtered/materialised
// afterwards (result()). pbbs times only `R = suffixArray(ss)` and writes the
// result after time_loop - so the filter + int64 materialisation must be
// outside the timed region too.

#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <parlay/sequence.h>

namespace student {

    struct SAContext {
        virtual ~SAContext() = default;
        virtual void run() = 0;
        // SA[i] = start index of the i-th smallest suffix (length n).
        virtual std::vector<std::int64_t> result() const = 0;
    };

    std::unique_ptr<SAContext> build_sa(const parlay::sequence<unsigned char>& s);

} // namespace student
