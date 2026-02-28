#include <cmath>
#include <scan.h>
#include <vector>
#include <par/pragma.h>


namespace parallel {

    template<typename T>
    void omp_scan(size_t n, const T* in, T* out);

    void scan(const std::vector<long long>& array, std::vector<long long>& result) {
        omp_scan(array.size(), array.data(), result.data());
    }


    template<typename T>
    void omp_scan(const size_t n, const T* in, T* out) {
        const int num_threads = par::max_threads();
        const int chunk = static_cast<int>(ceil(static_cast<double>(n) / num_threads));

        long long last_value_chunk_array[num_threads + 1];

        OMP_PARALLEL(default(none) shared(in, out, chunk, num_threads, last_value_chunk_array, n))
        {
            const int idThread = par::thread_id();

            OMP_SINGLE()
            {
                last_value_chunk_array[0] = 0;
            }

            long long operation = 0;

            OMP_FOR(schedule(static, chunk) nowait)
            for(int i = 0; i < n; i++) {
                if((i % chunk) == 0) {
                    operation = in[i];
                    out[i] = in[i];
                } else {
                    out[i] = out[i - 1] + in[i];
                    operation = operation + in[i];
                }
            }

            last_value_chunk_array[idThread + 1] = operation;

            OMP_BARRIER;

            long long balance = last_value_chunk_array[1];

            for(int i = 2; i < (idThread + 1); i++)
                balance = balance + last_value_chunk_array[i];

            OMP_FOR(schedule(static, chunk))
            for(int i = 0; i < n; i++) {
                if(idThread != 0) {
                    out[i] = out[i] + balance;
                }
            }
        }
    }
}