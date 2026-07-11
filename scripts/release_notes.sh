#!/usr/bin/env bash
# release_notes.sh: print the release body for a tag on stdout.
#
#   ./scripts/release_notes.sh v0.7.0
#
# The body is that version's section from CHANGELOG.md, followed by the standing
# download-and-run instructions every release carries. release.yml runs this twice:
# once in verify-version, where a missing section fails the tag before anything is
# built, and once in publish, to write the notes.
set -euo pipefail

if [ $# -ne 1 ]; then
  echo "usage: $0 <tag>" >&2
  exit 2
fi

tag="$1"
version="${tag#v}"
root="$(cd "$(dirname "$0")/.." && pwd)"
changelog="$root/CHANGELOG.md"

if [ ! -f "$changelog" ]; then
  echo "release_notes: missing $changelog" >&2
  exit 1
fi

# The section runs from its own "## vX.Y.Z" heading to the next "## v" heading (or
# the end of the file), the heading itself excluded: the release page shows the
# version in its title already.
section="$(awk -v want="## v$version" '
  $0 == want { found = 1; next }
  found && /^## v/ { exit }
  found { print }
' "$changelog")"

# Trim leading/trailing blank lines the split leaves behind.
section="$(printf '%s\n' "$section" | sed -e '/./,$!d' -e :a -e '/^\n*$/{$d;N;ba' -e '}')"

if [ -z "$section" ]; then
  echo "release_notes: CHANGELOG.md has no section for v$version" >&2
  echo "release_notes: add one under a '## v$version' heading" >&2
  exit 1
fi

cat <<EOF
## What's new

$section

---

Prebuilt binaries for \`$tag\`. Download one, make it executable, run it. The game
is a single static binary; there is nothing else to install.

| Platform | File |
| --- | --- |
| Linux (x86_64, glibc 2.35+) | \`mach-$tag-linux-x86_64\` |
| macOS (Apple silicon + Intel) | \`mach-$tag-macos-universal\` |
| Windows (x86_64) | \`mach-$tag-windows-x86_64.exe\` |

The binaries are unsigned. On macOS, clear the quarantine flag first:

    xattr -d com.apple.quarantine mach-$tag-macos-universal

On Windows, SmartScreen shows a warning: "More info" then "Run anyway".
EOF
