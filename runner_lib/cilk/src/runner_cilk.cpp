#include "runner.h"

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

        int tc = cfg.thread_count;
        runner::set_finish_hook([tc](runner::Meta& meta) {
            meta.max_threads_used = tc;
        });
    }

} // namespace runner
