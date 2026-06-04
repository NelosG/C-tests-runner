# C-tests-runner

C-tests-runner grades programming homework for a course on multithreaded algorithms. You
give it a student's solution and the teacher's test suite; it compiles both, runs the
tests in a sandbox, and hands back a JSON report with the running time, a set of
parallel-execution metrics, and a verdict for each test.

This is the part of the grading system that actually runs the code. The rest of it - the
web interface, the database, the GitLab integration - lives in a separate orchestrator
service (parallel-server) and isn't described here. Jobs reach the engine over RabbitMQ or
HTTP, and a single check can also be run straight from the command line.

## The check pipeline

A job is a student solution, a teacher test suite, and a set of resource limits. The engine
runs it through eight steps in order and stops at the first one that fails, recording where
it stopped:

1. **Resolve** - clone the solution and tests from Git (with a cache, so re-checks are
   cheap) or resolve a local path.
2. **Parse config** - read the assignment's `config.json`: grading mode, allowed
   frameworks and packages, resource caps.
3. **Detect framework** - work out which parallel framework the solution uses from its
   CMake files, and check it against the assignment's allowed list.
4. **Validate** - check the student's build files against a whitelist.
5. **Build runner** - compile the solution and link it into a test-runner executable.
6. **Build plugins** - compile the teacher's test plugins. These are cached per test
   directory, so a whole group submitting the same assignment only builds the tests once.
7. **Load plugins** - load the test DLLs and any declarative JSON test cases.
8. **Run tests** - run each test in the sandbox, collect output and metrics, and assemble
   the result.

The report is one JSON document: overall status, per-step progress, each test's result and
metrics, and a scalability summary across thread counts. Its shape is the same however the
job arrived.

## Metrics and run modes

A result is more than pass or fail. With monitoring on, each test records the running time
of the timed section, the work (`T1`) and span (`T_inf`) tracked as a DAG with a wall-clock
fallback, speedup and efficiency over a range of thread counts, 17 counters grouped by the
type of parallel construct used, and peak memory.

The same test can run three ways. Normal mode adds no instrumentation and is what timing
uses. Monitor mode counts constructs and measures work and span. Stress mode sprinkles
small random delays at synchronization points to shake out races.

## Frameworks and tests

A solution may use OpenMP, ParlayLib, or Cilk - the engine figures out which from the CMake
files and links the matching runtime. An assignment can also require plain sequential code
with no framework at all.

Teachers write tests in one of two ways. Plugin tests are C++ compiled into DLLs, with full
control over setup and checking. Declarative tests are JSON files giving input and expected
output, with no C++ needed. Either way they drive the same runner.

## Isolation

Student code is untrusted, so it is boxed in at two points.

During the build, a static scan of the CMake files rejects anything outside the
assignment's whitelist, a CMake wrapper disables the dependency-download calls, and the
compiler itself runs in a network namespace with no route out.

At run time on Linux every test runs under `isolate`. A cgroup caps memory, CPU time, and
process count. Mount, PID, network, and user namespaces leave the run with an empty
filesystem - apart from read-only mounts of the runner and input and a writable output
directory - no view of host processes, and no network, executing as a throwaway
unprivileged user. The cores it runs on are chosen (via hwloc) to keep all threads on one
cache and NUMA domain, so timings don't drift between runs. On Windows, which is used only
for development, a Win32 Job Object does the equivalent.

## Transports and code sources

Both the transports and the source providers are DLLs that can be loaded or swapped while
the engine is running. The transports are HTTP and RabbitMQ; the sources are Git (clone
with a persistent cache) and the local filesystem.

## Architecture

The codebase keeps a small, dependency-free plugin API that teacher tests compile against
apart from the heavier engine that does the work.

```
                 transport adapters (DLL)          resource providers (DLL)
                 HTTP  /  RabbitMQ                  Git  /  local filesystem
                        \                              /
                         v                            v
                   +-------------------------------------------+
                   |          server_core (the engine)         |
                   |                                           |
                   |  Pipeline      - 8-step job orchestration |
                   |  BuildService  - CMake compilation        |
                   |  Sandbox       - isolate / Job Object     |
                   |  CpuIsolator   - NUMA / L3 core pinning   |
                   |  Validators    - framework + CMake checks |
                   +-------------------------------------------+
                          |                          |
                          v                          v
                   test_engine (plugin API)   runner_lib (test harness
                   Test, TestRegistry,        linked into the student's
                   TestData                   test executable)
                          |
                          v
                   parallel_lib  - OpenMP instrumentation that
                                   produces the parallel metrics
```

- **test_engine** - the C++17 plugin API teacher tests link against: the `Test` interface,
  the test registry, and the binary input/output container.
- **server_core** - the engine: pipeline, CMake build service, sandbox launcher, CPU/NUMA
  isolator, validators, result building.
- **parallel_lib** - the OpenMP wrapper that produces the metrics.
- **runner_lib** - the harness linked into each student test executable, with one variant
  per framework (OpenMP, ParlayLib, Cilk, sequential) selected at link time.
- **adapters** - the HTTP and RabbitMQ transport DLLs.
- **resource** - the Git and local-filesystem source-provider DLLs.

## Repository layout

```
server.cpp / cli.cpp     entry points (server mode / single-run CLI)
test_engine/             plugin API: Test, TestRegistry, TestData (pure C++17)
server_core/             the engine - pipeline, build, sandbox, CPU isolation, results
parallel_lib/            OpenMP instrumentation; the 17-counter metric collection
runner_lib/              test-runner harness + per-framework variants (omp/parlay/cilk/seq)
adapters/http/           REST transport adapter
adapters/rabbit/         RabbitMQ transport adapter
resource/git/            clone-with-cache source provider
resource/local/          local-path source provider
examples/                sample assignments, including a port of the pbbs benchmarks
docker/                  Dockerfile, entrypoint, compose - the production deployment
tools/                   mock orchestrators for local development
```

## Building

Production runs on Linux, where the Docker image bundles every dependency. Development also
works on Windows with MSYS2 MinGW64.

```bash
# configure once
cmake -S . -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug

# build everything
cmake --build cmake-build-debug -j
```

A caveat for Windows development: the ParlayLib work-stealing scheduler can hang under
MinGW threads, so validate ParlayLib solutions on Linux or WSL.

## Running: server and CLI

The engine runs in two ways.

**Server mode** is how it works inside the grading system. It loads the transport adapters
and resource providers, registers itself as a node with the orchestrator, and waits for
jobs until it is stopped. While it runs it can be managed over the same transport - check
the queue, cancel a job, load or unload an adapter - without a restart. Incoming jobs run
in two lanes: correctness checks run concurrently, while a performance check runs on its
own and does not overlap anything else, so a timing run has the machine to itself.

**CLI mode** runs a check directly, with no adapters or transport, and prints the JSON
result. It takes one solution, or several against the same test suite, which is handy for
local runs and debugging.

```bash
# run a server node that receives jobs over an adapter
./server --node-id <id>

# run a single check locally
./cli --test-dir <tests> --test-id <id> --solution <solution-dir> [--threads 4]

# run several solutions against the same tests
./cli --test-dir <tests> --test-id <id> --solution <dir-a> --solution <dir-b>
```

The `examples/` directory is the quickest way to see a check end to end - `quick-sort-example`,
for instance, ships OpenMP, ParlayLib, and Cilk variants of one solution together with a
teacher test suite.

## Configuration

In server mode the engine reads JSON config files from a config directory; ready-to-edit
samples sit in `example-config/`. The server file is always read, plus one file for each
adapter and resource provider that is turned on:

- **server.json** - the node id, which adapters and resource providers load on startup, how
  many correctness checks run at once, the default resource limits (memory, threads,
  wall-clock and CPU time), and the sandbox and CPU-isolation options - path to `isolate`,
  how many cores to keep aside for the engine itself, which NUMA node to prefer.
- **http.json** - the HTTP adapter: host and port to listen on, the orchestrator's
  registration URL, and an API key.
- **rabbit.json** - the RabbitMQ adapter: broker host, port, credentials, and vhost.
- **resource-git.json** - the Git source: where the clone cache lives and how long it is
  kept.
- **resource-local.json** - the local source: the base directories that solutions and tests
  are resolved against.

Limits set per assignment or per request override the server defaults.
