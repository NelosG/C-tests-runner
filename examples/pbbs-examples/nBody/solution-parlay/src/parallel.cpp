// Parlay-native CK n-body. Particle array built outside the timed region
// (matches pbbs's nBody bench, which builds particles before time_loop and
// times only stepBH).

#include <nbody.h>
#include <parlay/primitives.h>
#include <pbbs_ck_nbody.h>

namespace {

    struct NbodyCtxImpl : student::NbodyContext {
        parlay::sequence<pbbs_ck_nbody::particle> ps;
        parlay::sequence<pbbs_ck_nbody::particle*> p;
        std::size_t n;

        NbodyCtxImpl(const parlay::sequence<double>& positions,
                     const parlay::sequence<double>& masses) {
            n = masses.size();
            const double* pp = positions.data();
            const double* mp = masses.data();
            ps = parlay::tabulate(n, [pp, mp](std::size_t i) {
                return pbbs_ck_nbody::particle(
                    pbbs_ck_nbody::point(pp[i*3], pp[i*3+1], pp[i*3+2]), mp[i]);
            });
            auto* base = ps.data();
            p = parlay::tabulate(n, [base](std::size_t i) { return base + i; });
        }

        void run() override {
            // ALPHA is a macro (#define ALPHA 2.6) inside pbbs_ck_nbody.h,
            // not a namespaced constant - use it unqualified.
            pbbs_ck_nbody::stepBH(p, ALPHA);
        }

        parlay::sequence<double> result() const override {
            return parlay::tabulate(n * 3, [this](std::size_t idx) -> double {
                const auto& f = ps[idx / 3].force;
                std::size_t c = idx % 3;
                return (c == 0) ? f.x : (c == 1) ? f.y : f.z;
            });
        }
    };

}

namespace student {

    std::unique_ptr<NbodyContext> build_nbody(
        const parlay::sequence<double>& positions,
        const parlay::sequence<double>& masses)
    {
        return std::make_unique<NbodyCtxImpl>(positions, masses);
    }

} // namespace student
