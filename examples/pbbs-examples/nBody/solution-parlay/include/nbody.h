// Student API for nBody. Computes the gravitational force on each
// particle from all other particles using the Callahan-Kosaraju (CK)
// algorithm (Barnes-Hut-style well-separated decomposition with
// spherical-harmonic multipole/local translations) vendored from pbbs.
//
// Parlay-native, 3-phase like pbbs's nBody bench: the particle array is
// built OUTSIDE the timed region (build_nbody), only stepBH is timed
// (NbodyContext::run), forces extracted afterwards (result()).

#pragma once

#include <memory>

#include <parlay/sequence.h>

namespace student {

    struct NbodyContext {
        virtual ~NbodyContext() = default;
        virtual void run() = 0;
        // 3*n force components, Output[i*3 .. i*3+3) = force on particle i.
        virtual parlay::sequence<double> result() const = 0;
    };

    std::unique_ptr<NbodyContext> build_nbody(
        const parlay::sequence<double>& positions,  // n*3
        const parlay::sequence<double>& masses);    // n

} // namespace student
