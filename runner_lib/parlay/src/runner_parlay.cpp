#include "runner.h"

#include <cstdlib>
#include <parlay/parallel.h>
#include <string>


namespace runner {

    void setup() {
        const auto& cfg = runner::config();
        std::string workers = std::to_string(cfg.thread_count);

        #ifdef _WIN32
        _putenv_s("PARLAY_NUM_THREADS", workers.c_str());
        #else
        setenv("PARLAY_NUM_THREADS", workers.c_str(), 1);
        #endif

        // Pre-init Parlay's work-stealing scheduler. The first parallel_for
        // spawns worker threads and allocates their work-stealing deques -
        // doing that here keeps the cost out of the timed region. `volatile`
        // prevents the optimizer from collapsing the loop body away.
        parlay::parallel_for(
            0,
            parlay::num_workers(),
            [](size_t i) {
                volatile size_t x = i;
                (void)x;
            }
        );

        int tc = cfg.thread_count;
        runner::set_finish_hook(
            [tc](runner::Meta& meta) {
                meta.max_threads_used = tc;
            }
        );
    }

} // namespace runner
