#!/usr/bin/env bash
# Count C/H source lines, split into blank / comment / code / total.
#
# Handles // line comments and /* ... */ block comments that span lines, counting
# a line as code if real code trails a closing */. Skips build/ and third_party/.
#
# Usage:
#   ./scripts/stats.sh [path] [--per-file]
#
# Examples:
#   ./scripts/stats.sh
#   ./scripts/stats.sh src --per-file

set -euo pipefail

root="."
per_file=0
for arg in "$@"; do
  case "$arg" in
    --per-file) per_file=1 ;;
    -h|--help) sed -n '2,14p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "Unknown option: $arg" >&2; exit 1 ;;
    *) root="$arg" ;;
  esac
done

if [ ! -d "$root" ]; then
  echo "Error: '$root' is not a directory" >&2
  exit 1
fi

# Source files, pruning the noise directories.
files=$(find "$root" \
  \( -name build -o -name third_party -o -name .git \) -prune -o \
  -type f \( -name '*.c' -o -name '*.h' \) -print)

if [ -z "$files" ]; then
  echo "No .c/.h files under '$root'"
  exit 0
fi

# One awk pass builds per-file counts; rows are tagged F (file) or T (totals) so
# the shell can sort files by size and print totals separately. Portable awk only
# (no gawk extensions), since macOS ships BSD awk.
# shellcheck disable=SC2086
counted=$(awk '
  FNR == 1 { in_block = 0; nf++ }
  {
    line = $0
    sub(/^[ \t]+/, "", line); sub(/[ \t]+$/, "", line)   # trim
    if (line == "") { blank[FILENAME]++; tb++; next }

    if (in_block) {
      comment[FILENAME]++; tc++
      p = index(line, "*/")
      if (p) {
        in_block = 0
        rest = substr(line, p + 2)
        sub(/^[ \t]+/, "", rest); sub(/[ \t]+$/, "", rest)
        if (rest != "" && substr(rest, 1, 2) != "//") {
          comment[FILENAME]--; tc--; code[FILENAME]++; tk++
        }
      }
      next
    }
    if (substr(line, 1, 2) == "//") { comment[FILENAME]++; tc++; next }
    if (substr(line, 1, 2) == "/*") {
      comment[FILENAME]++; tc++
      if (!index(line, "*/")) in_block = 1
      next
    }
    code[FILENAME]++; tk++
  }
  END {
    for (f in blank)   seen[f] = 1
    for (f in comment) seen[f] = 1
    for (f in code)    seen[f] = 1
    for (f in seen) {
      c = code[f] + 0; m = comment[f] + 0; b = blank[f] + 0
      printf "F\t%d\t%d\t%d\t%d\t%s\n", c, m, b, c + m + b, f
    }
    printf "T\t%d\t%d\t%d\t%d\t%d\n", nf + 0, tk + 0, tc + 0, tb + 0, tk + tc + tb + 0
  }
' $files)

if [ "$per_file" -eq 1 ]; then
  printf '%-55s %8s %8s %8s %8s\n' "File" "Code" "Comment" "Blank" "Total"
  printf -- '-%.0s' $(seq 1 96); printf '\n'
  echo "$counted" | awk -F'\t' '$1 == "F"' | sort -t "$(printf '\t')" -k5 -nr |
    awk -F'\t' '{ printf "%-55s %8d %8d %8d %8d\n", $6, $2, $3, $4, $5 }'
  printf -- '-%.0s' $(seq 1 96); printf '\n'
fi

echo "$counted" | awk -F'\t' '$1 == "T" {
  printf "\nScanned %d file(s) under '\''%s'\''\n\n", $2, "'"$root"'"
  printf "%-15s: %d\n", "Code lines", $3
  printf "%-15s: %d\n", "Comment lines", $4
  printf "%-15s: %d\n", "Blank lines", $5
  printf "%-15s: %d\n", "Total lines", $6
}'
