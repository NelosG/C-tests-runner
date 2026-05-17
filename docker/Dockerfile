# syntax=docker/dockerfile:1.4
# (1.4+ enables the RUN --mount=type=cache directive used below.)

# ============================================
# Stage 1: Build engine
# ============================================
FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    cmake \
    ninja-build \
    git \
    pkg-config \
    libomp-dev \
    libnuma-dev \
    libuv1-dev \
    libhwloc-dev \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

COPY CMakeLists.txt main_common.h server.cpp cli.cpp ./
COPY cmake/ cmake/
COPY test_engine/ test_engine/
COPY server_core/ server_core/
COPY parallel_lib/ parallel_lib/
COPY runner_lib/ runner_lib/
COPY adapters/ adapters/
COPY resource/ resource/
# tools/ and tests/ are dev-only (mock orchestrators, example test plugins,
# sample student solution). Not copied into the production image; the engine
# build skips them via -DBUILD_DEV_EXTRAS=OFF.

# BuildKit cache mount: the CMake build tree persists across `docker compose
# build` invocations in BuildKit's storage (NOT in the final image). When one
# .cpp changes, Ninja recompiles only the affected TUs + re-links - turns a
# 3+ min full rebuild into ~30s incremental. `sharing=locked` serialises
# concurrent docker builds on the same cache. The install step writes into
# /opt/ctr (regular layer), so the artifacts end up in the image as before.
RUN --mount=type=cache,target=/src/build,sharing=locked \
    cmake -S . -B /src/build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/ctr \
        -DBUILD_DEV_EXTRAS=OFF \
    && cmake --build /src/build -j$(nproc) \
    && cmake --install /src/build

# ============================================
# Stage 2: Install OpenCilk (reusable layer)
# ============================================
FROM ubuntu:24.04 AS opencilk

RUN apt-get update && apt-get install -y --no-install-recommends wget ca-certificates \
    && cd /tmp \
    && wget -q https://github.com/OpenCilk/opencilk-project/releases/download/opencilk/v3.0/opencilk-3.0.0-x86_64-linux-gnu-ubuntu-24.04.tar.gz \
    && tar xzf opencilk-3.0.0-x86_64-linux-gnu-ubuntu-24.04.tar.gz -C /opt \
    && mv /opt/opencilk-* /opt/opencilk \
    && rm -rf /tmp/* /var/lib/apt/lists/*

# ============================================
# Stage 3: Build isolate sandbox (IOI sandbox)
# ============================================
FROM ubuntu:24.04 AS isolate

# Pinned to v2.5 (matches our WSL reference install).
# v2.3+ added seccomp syscall filtering (needs libseccomp-dev) and renamed the
# config layout: CONFIG is now a single file ($CONFIGDIR/isolate), not a directory.
# We build the three program targets directly and skip Makefile's `all` /
# `install` rules - those try to render manpages via a2x/xmllint, which need
# docbook-xml/xsl catalogs not shipped in the slim Ubuntu base image.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential libcap-dev libsystemd-dev libseccomp-dev pkg-config \
        git ca-certificates \
    && git clone https://github.com/ioi/isolate.git /tmp/isolate \
    && cd /tmp/isolate \
    && git checkout v2.5 \
    && make -j"$(nproc)" PREFIX=/usr CONFIGDIR=/etc CONFIG=/etc/isolate \
        isolate isolate-cg-keeper isolate-check-environment default.cf \
    && install -d /opt/isolate-stage/usr/bin /opt/isolate-stage/usr/sbin \
                  /opt/isolate-stage/etc /opt/isolate-stage/var/local/lib/isolate \
    && install -m 4755 isolate                   /opt/isolate-stage/usr/bin/ \
    && install        isolate-check-environment  /opt/isolate-stage/usr/bin/ \
    && install        isolate-cg-keeper          /opt/isolate-stage/usr/sbin/ \
    && install -m 644 default.cf                 /opt/isolate-stage/etc/isolate \
    && rm -rf /var/lib/apt/lists/*

# ============================================
# Stage 4: Runtime
# ============================================
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ \
    cmake \
    ninja-build \
    git \
    wget \
    libgomp1 \
    libnuma1 \
    libuv1 \
    libhwloc15 \
    libcap2 \
    libsystemd0 \
    libseccomp2 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Layer ordering rule: stable layers FIRST, the volatile builder COPY LAST.
# Layer cache invalidation cascades down - putting the builder copy at the
# bottom keeps everything above it cached when only C++ source changes.

# isolate (IOI sandbox) - overlay full staged install (binary + helpers + config + box dir)
COPY --from=isolate /opt/isolate-stage/ /
# v2.3+ requires an 'isolate' user with a sub[ug]id range for user-namespace
# UID mapping. Replicates what isolate's debian/isolate.postinst does at apt install.
RUN chmod u+s /usr/bin/isolate \
    && groupadd --system isolate \
    && useradd --system --gid isolate --home-dir /nonexistent --no-create-home \
               --shell /bin/false isolate \
    && echo 'isolate:100000:65536' >> /etc/subuid \
    && echo 'isolate:100000:65536' >> /etc/subgid

# OpenCilk compiler (~1 GB, stable across C++ source changes - placed here
# so the COPY is cached on every rebuild that doesn't touch the opencilk stage).
COPY --from=opencilk /opt/opencilk /opt/opencilk

# Config dir: empty placeholder for the docker-compose volume mount
# (docker/config/ -> /opt/ctr/config/). Done before the builder COPY so the
# directory exists; COPY merges /opt/ctr from the builder alongside this dir.
RUN mkdir -p /opt/ctr/config

ENV PATH="/opt/opencilk/bin:/opt/ctr:${PATH}" \
    LD_LIBRARY_PATH="/opt/ctr:/opt/opencilk/lib" \
    OPENCILK_PATH="/opt/opencilk"

EXPOSE 8090

HEALTHCHECK --interval=30s --timeout=5s --retries=3 \
    CMD wget -qO- http://localhost:8090/api/health >/dev/null 2>&1 || exit 1

COPY docker/entrypoint.sh /entrypoint.sh
# Strip CR from CRLF line endings (Windows hosts with core.autocrlf=true ship
# the file with CRLF, which makes the kernel see `#!/bin/sh\r` and fail with
# "exec /entrypoint.sh: no such file or directory").
RUN sed -i 's/\r$//' /entrypoint.sh && chmod +x /entrypoint.sh

# Engine binaries + libs + headers - VOLATILE: invalidates on every C++ source
# change. Kept last so all stable layers above stay cached on rebuild.
COPY --from=builder /opt/ctr /opt/ctr

ENTRYPOINT ["/entrypoint.sh"]
CMD ["server"]
