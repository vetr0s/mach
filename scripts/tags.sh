#!/usr/bin/env bash
set -euo pipefail

# Generate ctags for Emacs
# Usage: ./scripts/tags.sh

cd "$(dirname "$0")/.."

echo "Generating tags for src/..."
ctags -e -R src/

echo "Tags generated: $(pwd)/TAGS"
echo ""
echo "Add this to your Emacs init:"
echo "  (setq tags-file-name \"$(pwd)/TAGS\")"
echo "  (define-key global-map \"\\C-]\" 'find-tag)"
echo "  (define-key global-map \"\\C-\\M-]\" 'pop-tag-mark)"
