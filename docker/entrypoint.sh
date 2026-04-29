#!/bin/sh
set -e

# --- Sandbox setup ---------------------------------------------------------
# isolate (IOI sandbox) v2.3+ reads its cgroup root path from
# /run/isolate/cgroup (config: `cg_root = auto:/run/isolate/cgroup`).
# Normally `isolate-cg-keeper.service` (systemd) populates this file.
# In a container without systemd we replicate that manually: enable the
# cgroup v2 controllers we need, create a dedicated `isolate` sub-cgroup,
# and write its absolute path so isolate can use it as its root.
if [ -d /sys/fs/cgroup ] && [ -w /sys/fs/cgroup ]; then
    # Enable controllers in the root cgroup so they propagate to children.
    echo "+cpu +cpuset +io +memory +pids" > /sys/fs/cgroup/cgroup.subtree_control 2>/dev/null || true
    mkdir -p /sys/fs/cgroup/isolate
    # Also enable controllers in /sys/fs/cgroup/isolate so isolate's per-box
    # children (box-0, box-1, ...) can have memory.max / cpu.max / cpuset.cpus
    # files of their own. Without this isolate fails with
    # "Cannot write /sys/fs/cgroup/isolate/box-N/memory.max: No such file".
    echo "+cpu +cpuset +io +memory +pids" > /sys/fs/cgroup/isolate/cgroup.subtree_control 2>/dev/null || true
    mkdir -p /run/isolate
    echo "/sys/fs/cgroup/isolate" > /run/isolate/cgroup
else
    echo "[entrypoint] Warning: /sys/fs/cgroup not writable - sandbox will fail." >&2
    echo "[entrypoint] Run with 'privileged: true' and 'cgroup: host' (see docker-compose.yml)." >&2
fi

case "${1:-server}" in
    server)
        shift 2>/dev/null || true
        exec /opt/ctr/server --node-id "${NODE_ID:-docker-node}" "$@"
        ;;
    cli)
        shift
        exec /opt/ctr/cli "$@"
        ;;
    *)
        echo "Usage: docker run <image> [server|cli] [args...]"
        echo "  server (default): long-running server with adapters"
        echo "  cli:              one-shot test run, outputs JSON"
        exit 1
        ;;
esac
