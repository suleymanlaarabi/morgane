#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
export PATH="$project_root/.tools/bin:$PATH"
vcpkg_dir="$project_root/.tools/vcpkg"
vcpkg_revision="$(sed -n 's/.*"builtin-baseline": "\([0-9a-f]*\)".*/\1/p' "$project_root/vcpkg.json")"
if [[ ! "$vcpkg_revision" =~ ^[0-9a-f]{40}$ ]]; then
    echo "Expected one pinned builtin-baseline in vcpkg.json." >&2
    exit 1
fi
for tool in git curl zip unzip tar cmake ninja python3; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "Missing build tool: $tool. See README.md for prerequisites." >&2
        exit 1
    fi
done

bootstrap_required=false
if [[ ! -d "$vcpkg_dir/.git" ]]; then
    git init -q "$vcpkg_dir"
fi
current_revision="$(git -C "$vcpkg_dir" rev-parse --verify HEAD 2>/dev/null || true)"
if [[ "$current_revision" != "$vcpkg_revision" ]]; then
    if ! git -C "$vcpkg_dir" diff --quiet || ! git -C "$vcpkg_dir" diff --cached --quiet; then
        echo "Local changes in $vcpkg_dir; refusing to replace them." >&2
        exit 1
    fi
    git -C "$vcpkg_dir" fetch --depth 1 https://github.com/microsoft/vcpkg.git "$vcpkg_revision"
    git -C "$vcpkg_dir" checkout --detach "$vcpkg_revision"
    bootstrap_required=true
fi
if [[ ! -x "$vcpkg_dir/vcpkg" || "$bootstrap_required" == true ]]; then
    "$vcpkg_dir/bootstrap-vcpkg.sh" -disableMetrics
fi
printf 'Ready. Run: cmake --preset linux-debug\n'
