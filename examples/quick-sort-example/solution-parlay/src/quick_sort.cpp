#include <quick_sort.h>

#include <algorithm>
#include <parlay/parallel.h>
#include <parlay/primitives.h>
#include <parlay/sequence.h>


namespace parallel {

    constexpr long BLOCK = 1'000;

    static void do_qsort(long long* a, long lo, long hi) {
        if(hi - lo < BLOCK) {
            std::sort(a + lo, a + hi + 1);
            return;
        }

        // Median-of-three pivot to avoid worst case on sorted-ish input.
        long mid = lo + (hi - lo) / 2;
        if(a[lo] > a[mid]) std::swap(a[lo], a[mid]);
        if(a[lo] > a[hi]) std::swap(a[lo], a[hi]);
        if(a[mid] > a[hi]) std::swap(a[mid], a[hi]);
        std::swap(a[mid], a[hi - 1]);
        long long pivot = a[hi - 1];

        long i = lo;
        long j = hi - 1;
        while(true) {
            while(a[++i] < pivot) {}
            while(a[--j] > pivot) {}
            if(i >= j) break;
            std::swap(a[i], a[j]);
        }
        std::swap(a[i], a[hi - 1]);

        parlay::par_do(
            [&] { do_qsort(a, lo, i - 1); },
            [&] { do_qsort(a, i + 1, hi); }
        );
    }

    void qsort(std::vector<long long>& array) {
        if(array.size() < 2) return;
        do_qsort(array.data(), 0, static_cast<long>(array.size()) - 1);
    }
}
