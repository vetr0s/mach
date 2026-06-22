#!/usr/bin/env bash
# Convenience wrapper: configure + build a preset.
#   scripts/build.sh [preset]
# Defaults to <os>-debug for the host platform.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

case "$(uname -s)" in
  Darwin) default_preset="macos-debug" ;;
  Linux)  default_preset="linux-debug" ;;
  *)      default_preset="" ;;
esac

preset="${1:-$default_preset}"
if [ -z "$preset" ]; then
  echo "Could not infer a preset for this platform; pass one explicitly." >&2
  exit 1
fi

cmake --preset "$preset"
cmake --build --preset "$preset"
