# ============================================
# Stage 1: Build
# ============================================
FROM gcc:12 AS builder

RUN apt-get update && apt-get install -y \
    cmake \
    ninja-build \
    libomp-dev \
    && rm -rf /var/lib/apt/lists/*

COPY CMakeLists.txt server_main.cpp main.cpp test_main.cpp /src/
COPY test_engine/ /src/test_engine/
COPY parallel_lib/ /src/parallel_lib/
COPY tests/solution/ /src/solution/
COPY tests/ /src/tests/

WORKDIR /src/build

RUN cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/opt/test-runner \
    && ninja \
    && ninja install

# ============================================
# Stage 2: Runtime
# ============================================
FROM gcc:12

RUN apt-get update && apt-get install -y \
    libomp-dev \
    numactl \
    cmake \
    ninja-build \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /opt/test-runner /opt/test-runner

ENV LD_LIBRARY_PATH=/opt/test-runner/lib
ENV PATH="/opt/test-runner/bin:${PATH}"
ENV ENGINE_LIB_PATH=/opt/test-runner/lib/libtest_engine.so
ENV ENGINE_INCLUDE_PATH=/opt/test-runner/include
ENV PARALLEL_LIB_PATH=/opt/test-runner/lib/libparallel_lib.so
ENV PARALLEL_INCLUDE_PATH=/opt/test-runner/include

EXPOSE 8080

CMD ["/opt/test-runner/bin/server"]
