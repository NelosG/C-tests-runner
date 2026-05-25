// Student API for convexHull. One parallel variant: pbbsbench's
// quickHull (recursive divide-and-conquer).
//
// Parlay-native, 3-phase like pbbs's hull bench: the point sequence is built
// OUTSIDE the timed region (build_hull), only quickHull is timed
// (HullContext::run), hull indices extracted afterwards (result()). pbbs
// builds the point sequence before time_loop and times only hull(pts).

#pragma once

#include <cstdint>
#include <memory>

#include <parlay/sequence.h>

namespace student {

    struct HullContext {
        virtual ~HullContext() = default;
        virtual void run() = 0;
        virtual parlay::sequence<std::int64_t> result() const = 0;
    };

    std::unique_ptr<HullContext> build_hull(
        const parlay::sequence<double>& xs,
        const parlay::sequence<double>& ys);

} // namespace student
