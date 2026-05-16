#include "runner.h"

#include <cilk/cilk.h>
#include <cstdlib>
#include <string>


namespace runner {

    void setup() {
        const auto& cfg = runner::config();
        std::string workers = std::to_string(cfg.thread_count);

        #ifdef _WIN32
        _putenv_s("CILK_NWORKERS", workers.c_str());
        #else
        setenv("CILK_NWORKERS", workers.c_str(), 1);
        #endif

        // Pre-init Cilk worker pool. The first cilk_for in student code would
        // otherwise pay runtime init + worker spawn + cactus stack allocation.
        // This source file is compiled at student-build time with the OpenCilk
        // clang BuildService picks for the cilk framework, so cilk_for is
        // legal here.
        cilk_for(int i = 0;
        i < cfg.thread_count;
        ++i
        )
        {
            volatile int x = i;
            (void)x;
        }

        int tc = cfg.thread_count;
        runner::set_finish_hook(
            [tc](runner::Meta& meta) {
                meta.max_threads_used = tc;
            }
        );
    }

} // namespace runner
