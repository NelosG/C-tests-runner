#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <pbbs_parallel_ks.h>
#include <sa.h>

namespace {

    struct SACtxImpl : student::SAContext {
        // s lives in the runner main for the whole run; suffixArray takes it
        // by const ref and does not mutate it, so a reference is safe across
        // the (warmup + timed) iterations.
        const parlay::sequence<unsigned char>& s;
        parlay::sequence<unsigned int> sa;

        explicit SACtxImpl(const parlay::sequence<unsigned char>& s_) : s(s_) {}

        void run() override {
            sa = pbbs_parallel_ks::suffixArray(s);
        }

        std::vector<std::int64_t> result() const override {
            // Drop the virtual empty suffix (index n); materialise to int64.
            // Outside the timed region - pbbs writes its output after time_loop.
            auto filt = parlay::filter(sa, [n = s.size()](auto x) { return x < n; });
            std::vector<std::int64_t> out(filt.size());
            std::int64_t* dst = out.data();
            parlay::parallel_for(0, filt.size(), [&](std::size_t i) {
                dst[i] = static_cast<std::int64_t>(filt[i]);
            });
            return out;
        }
    };

}

namespace student {

    std::unique_ptr<SAContext> build_sa(const parlay::sequence<unsigned char>& s) {
        return std::make_unique<SACtxImpl>(s);
    }

} // namespace student
