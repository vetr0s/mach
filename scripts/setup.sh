#!/usr/bin/env bash
# Fetch vendored dependencies (SDL3) after a fresh clone.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

git submodule update --init --recursive
echo "Submodules ready. Next: cmake --preset <platform>-debug && cmake --build --preset <platform>-debug"
