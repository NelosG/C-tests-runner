#include <cmath>
#include <iostream>
#include <ostream>
#include <scan.h>
#include <vector>
#include <par/pragma.h>


template<typename T>
void par_scan(size_t n, const T* in, T* out);

template<typename T>
void scan(const std::vector<T>& array, std::vector<T>& result) {
    par_scan(array.size(), array.data(), result.data());
}

template<typename T>
void par_scan(const size_t n, const T* in, T* out) {
    const int num_threads = par::max_threads();
    const int chunk = static_cast<int>(ceil(static_cast<double>(n) / num_threads));

    long long last_value_chunk_array[num_threads + 1];

    OMP_PARALLEL(default(shared))
    {
        const int idThread = par::thread_id();

        OMP_SINGLE()
        {
            last_value_chunk_array[0] = 0;
        }

        long long operation = 0;

        // Phase 1: local prefix sum within each chunk
        OMP_FOR(schedule(static) nowait)
        for(int i = 0; i < static_cast<int>(n); ++i) {
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

        // Phase 2: compute balance from preceding chunks
        long long balance = last_value_chunk_array[1];
        for(int i = 2; i < (idThread + 1); i++)
            balance = balance + last_value_chunk_array[i];

        // Phase 3: add balance to all elements (except thread 0)
        OMP_FOR(schedule(static))
        for(int i = 0; i < static_cast<int>(n); ++i) {
            if(idThread != 0) {
                out[i] = out[i] + balance;
            }
        }
    }
}

Scan::InternalScan Scan::getInternal() const {
    return InternalScan(*this);
}

class NotImplemented {};

class Scan::InternalNotImplemented {};

class Scan::InternalScan::InternalInternal::InternalInternalInternalNotImplemented {};

class stubtest::NotImplemented {};

void Scan::getScanned(std::vector<long long>& result) const {
    scan(array, result);
}

template<typename T, typename R>
class Scan::GenericNotImplemented {
    GenericNotImplemented(T t, R r) : t(t), r(r) {}
    T t;
    R r;
};

void Scan::acceptGenericConst(Generic<int, long> t) const {
    std::cout << "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        << t.t << t.r << '\n';
}