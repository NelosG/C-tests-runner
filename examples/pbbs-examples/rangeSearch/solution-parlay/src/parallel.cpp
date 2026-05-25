// Wrapper: high-dim float vectors -> pbbs Tvec_point sequence ->
// build HCNNG index -> rangeSearchRandom per query -> read q->ngh.
// Returns pbbs's flat per-query result layout (see rs.h).

#include <pbbs_hcnng_range.h>
#include <rs.h>

namespace student {

    namespace {

        // HCNNG parameters mirror pbbs/benchmarks/rangeSearch/HCNNG/range.sh
        // (mstDeg / num_clusters / cluster_size / beamSize). Larger
        // values give higher recall, slower build/query.
        constexpr int MSTdeg = 10;
        constexpr int num_clusters = 30;
        constexpr int cluster_size = 100;
        constexpr int beamSize = 200;

    } // namespace

    std::vector<long long> hcnng_range_search(
        const std::vector<float>& corpus,
        const std::vector<float>& queries,
        std::int64_t dim,
        double rad)
    {
        using namespace pbbs_hcnng_range;
        using T = float;
        using vtx = Tvec_point<T>;

        std::int64_t n = static_cast<std::int64_t>(corpus.size()) / dim;
        std::int64_t q = static_cast<std::int64_t>(queries.size()) / dim;
        unsigned d = (unsigned)dim;

        // pbbs's AVX L2 distance reads 8 floats per block; pad each
        // point's row to the next multiple of 8 so the over-read
        // touches zero padding instead of neighbour memory. (Same fix
        // as pbbs-examples/ANN/solution-parlay/src/parallel.cpp.)
        const unsigned stride = (d + 7u) & ~7u;

        sequence<T> corpus_pad(n * stride, T{});
        for(std::int64_t i = 0; i < n; ++i)
            for(unsigned j = 0; j < d; ++j)
                corpus_pad[i * stride + j] = corpus[i * d + j];

        sequence<T> queries_pad(q * stride, T{});
        for(std::int64_t i = 0; i < q; ++i)
            for(unsigned j = 0; j < d; ++j)
                queries_pad[i * stride + j] = queries[i * d + j];

        const int max_total_deg = MSTdeg * num_clusters + 64;
        sequence<int> nbh_buf(n * max_total_deg, -1);

        sequence<vtx> vv(n);
        for(std::int64_t i = 0; i < n; ++i) {
            vv[i].id = (int)i;
            vv[i].coordinates = parlay::make_slice(
                &corpus_pad[i * stride], &corpus_pad[i * stride] + d);
            vv[i].out_nbh = parlay::make_slice(
                &nbh_buf[i * max_total_deg],
                &nbh_buf[i * max_total_deg] + max_total_deg);
            vv[i].new_nbh = parlay::make_slice<int*, int*>(nullptr, nullptr);
        }
        sequence<vtx*> v = parlay::tabulate(n, [&](std::size_t i) {
            return &vv[i];
        });

        hcnng_index<T> idx(MSTdeg, d, /*mips=*/false);
        idx.build_index(v, num_clusters, (size_t)cluster_size);

        sequence<vtx> qq(q);
        for(std::int64_t i = 0; i < q; ++i) {
            qq[i].id = (int)i;
            qq[i].coordinates = parlay::make_slice(
                &queries_pad[i * stride], &queries_pad[i * stride] + d);
            qq[i].out_nbh = parlay::make_slice<int*, int*>(nullptr, nullptr);
            qq[i].new_nbh = parlay::make_slice<int*, int*>(nullptr, nullptr);
        }
        sequence<vtx*> qs = parlay::tabulate(q, [&](std::size_t i) {
            return &qq[i];
        });

        // pbbs's distance returns squared L2, so the radius argument
        // inside range_search is compared against r^2.
        double r2 = rad * rad;
        // `k` is the cap on returned neighbours per query; pbbs's range
        // helper still wants a sensible k for the underlying beam.
        int k_cap = 64;
        rangeSearchRandom<T>(qs, v, beamSize, d, r2, k_cap);

        // Flatten qs[i]->ngh into the pbbs layout in parallel via
        // tabulate over per-query sizes + scan for write offsets.
        auto sizes = parlay::tabulate(q,
            [&](std::size_t i) -> std::int64_t { return qs[i]->ngh.size(); });
        auto [offsets, total_ids] = parlay::scan(sizes);
        std::vector<long long> out(1 + q + total_ids);
        long long* dst = out.data();
        dst[0] = q;
        parlay::parallel_for(0, q, [&](std::size_t i) {
            dst[1 + i] = (long long)qs[i]->ngh.size();
        });
        long long* ids_base = dst + 1 + q;
        parlay::parallel_for(0, q, [&](std::size_t i) {
            long long off = offsets[i];
            const auto& ng = qs[i]->ngh;
            for(std::size_t j = 0; j < ng.size(); ++j)
                ids_base[off + j] = (long long)ng[j];
        });
        return out;
    }

} // namespace student
