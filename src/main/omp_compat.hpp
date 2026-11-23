#pragma once
// OpenMP when the compiler provides it, otherwise single-call fallbacks.
// iOS (and any toolchain without an OpenMP runtime) builds without -fopenmp;
// the "#pragma omp" loops then simply run serially.
#if defined(_OPENMP)
    #include <omp.h>
#else
    #include <algorithm>
    #include <thread>
    inline int omp_get_max_threads()
    {
        return static_cast<int>(std::max(1u, std::thread::hardware_concurrency()));
    }
    inline int omp_get_thread_num()  { return 0; }
    inline int omp_get_num_threads() { return 1; }
#endif
