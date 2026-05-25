// Wrapper: flat std::vector<float> -> pbbs Tvec_point sequence ->
// build HCNNG index -> beam_search for each point to get k-NN.
//
// pbbs's HCNNG/neighbors.h top-level `ANN` pulls in graph_stats /
// search_and_parse / recall reporting that aren't part of the
// algorithm. We dropped that file from the vendored header and drive
// hcnng_index + beam_search directly here.
#include <ann.h>
#include <pbbs_hcnng_ann.h>

namespace {

    // HCNNG parameters from pbbs ANN bench defaults (HCNNG/neighbors.sh).
    // pbbs ANN bench defaults are tuned for million-point corpora; for
    // our smaller test sizes (n down to 500) we widen the graph and the
    // query beam so recall is reasonable on tiny inputs.
    constexpr int MSTdeg = 10;       // out-edges per cluster MSF
    constexpr int num_clusters = 30; // clustering rounds
    constexpr int cluster_size = 100;
    constexpr int beamSize = 200;    // beam width during query

} // namespace

namespace student {

    std::vector<std::int64_t> hcnng_ann(
        std::int64_t k,
        std::int64_t dim,
        const std::vector<float>& points)
    {
        using namespace pbbs_hcnng_ann;
        using T = float;
        using vtx = Tvec_point<T>;

        std::int64_t n = points.size() / dim;
        unsigned d = (unsigned)dim;

        // pbbs's AVX L2 distance reads coords in 8-float blocks
        // (D = (size+7) & ~7U). For small d (e.g. 3) that over-reads past
        // the point's real coordinates into adjacent memory and ruins the
        // distance. Pad each point's row up to the next multiple of 8
        // floats; padding slots stay zero so the L2 diff contribution
        // is zero.
        const unsigned stride = (d + 7u) & ~7u;
        sequence<T> coords(n * stride, T{});
        for(std::int64_t i = 0; i < n; ++i)
            for(unsigned j = 0; j < d; ++j)
                coords[i * stride + j] = points[i * d + j];

        // generous capacity: MSTk can pump in up to MSTdeg neighbours
        // per round per vertex; 30 rounds and some duplication slack.
        const int max_total_deg = MSTdeg * num_clusters + 64;
        sequence<int> nbh_buf(n * max_total_deg, -1);

        sequence<vtx> vv(n);
        for(std::int64_t i = 0; i < n; ++i) {
            vv[i].id = (int)i;
            vv[i].coordinates = parlay::make_slice(
                &coords[i * stride], &coords[i * stride] + d);
            // Slice covers the WHOLE capacity buffer; pbbs's `size_of`
            // counts non-(-1) entries inside the slice as the current
            // degree, while slice.size() is the cap. The buffer was
            // pre-filled with -1 above so size_of() initially is 0.
            vv[i].out_nbh = parlay::make_slice(
                &nbh_buf[i * max_total_deg],
                &nbh_buf[i * max_total_deg] + max_total_deg);
            vv[i].new_nbh = parlay::make_slice<int*, int*>(nullptr, nullptr);
        }
        sequence<vtx*> v = parlay::tabulate(n, [&](std::size_t i) {
            return &vv[i];
        });

        // Build the HCNNG index (multiple_clustertrees + dedupe).
        hcnng_index<T> idx(MSTdeg, d, /*mips=*/false);
        idx.build_index(v, num_clusters, (size_t)cluster_size);

        // pbbs's `beamSearchRandom` fills v[i]->ngh for each i via random
        // starting points. Better recall than always starting from v[0].
        int q_beam = std::max((int)beamSize, (int)k + 1);
        beamSearchRandom<T>(v, v, q_beam, (int)k, d, /*mips=*/false);

        std::vector<std::int64_t> out(n * k, -1);
        for(std::int64_t i = 0; i < n; ++i) {
            const auto& ngh = v[i]->ngh;
            int written = 0;
            for(std::size_t j = 0; j < ngh.size() && written < k; ++j) {
                int id = ngh[j];
                if(id == (int)i) continue;
                out[i * k + written++] = id;
            }
        }
        return out;
    }

} // namespace student
