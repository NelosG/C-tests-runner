#include <algorithm>
#include <ctime>
#include <quick_sort.h>
#include <random>


namespace parallel {

    void qsort_tasks(std::vector<int>& array, int low, int high, std::mt19937& gen);

    void qsort(std::vector<int>& array) {
        std::mt19937 gen(clock());
        qsort_tasks(array, 0, static_cast<int>(array.size()) - 1, gen);
    }

    int partition(std::vector<int>& array, const int low, const int high) {
        const int pivot = array[high];

        int i = (low - 1);
        for(int j = low; j <= high - 1; j++) {
            if(array[j] <= pivot) {
                i++;
                std::swap(array[i], array[j]);
            }
        }
        std::swap(array[i + 1], array[high]);
        return (i + 1);
    }

    int partition_r(std::vector<int>& array, const int low, const int high, std::mt19937& gen) {
        const int mod = (high - low);
        const int random = low + (static_cast<int>(gen()) % mod + mod) % mod;
        std::swap(array[random], array[high]);

        return partition(array, low, high);
    }

    void do_qsort(std::vector<int>& array, int low, int high, std::mt19937& gen) {
        if(low < high) {
            int q = partition_r(array, low, high, gen);

            if(high - low < BLOCK) {
                do_qsort(array, low, q - 1, gen);
                do_qsort(array, q + 1, high, gen);
            } else {
                #pragma omp task default(none) shared(array, gen, q, low)
                do_qsort(array, low, q - 1, gen);
                #pragma omp task default(none) shared(array, gen, q, high)
                do_qsort(array, q + 1, high, gen);
            }
        }
    }

    void qsort_tasks(std::vector<int>& array, int low, int high, std::mt19937& gen) {
        #pragma omp parallel default(none) shared(array, low, high, gen)
        {
            #pragma omp single
            do_qsort(array, low, high, gen);
            #pragma omp taskwait
        }
    }
}
