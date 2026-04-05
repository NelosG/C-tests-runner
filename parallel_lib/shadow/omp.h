// Shadow omp.h -- blocks direct student use of <omp.h> when the assignment's
// allowedPackages does NOT contain "OpenMP" (engine activates this header via
// include_directories(BEFORE SYSTEM ...) only in that case).
//
// parallel_lib's pragma.h defines _PAR_OMP_INTERNAL before including <omp.h>,
// which triggers #include_next to forward to the real system omp.h.

#ifdef _PAR_OMP_INTERNAL
// Internal include from parallel_lib -- forward to real system omp.h
#include_next <omp.h>
#else
#error "Direct #include <omp.h> is forbidden by this assignment. Use <par/pragma.h> from parallel_lib, or ask the teacher to add \"OpenMP\" to allowedPackages."
#endif
