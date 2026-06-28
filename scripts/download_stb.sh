#!/usr/bin/env bash
set -euo pipefail

STB_DIR="third_party/stb"
STB_REPO="https://raw.githubusercontent.com/nothings/stb/master"

echo "Downloading stb headers..."

curl -o "$STB_DIR/stb_image.h" "$STB_REPO/stb_image.h"
curl -o "$STB_DIR/stb_truetype.h" "$STB_REPO/stb_truetype.h"
curl -o "$STB_DIR/stb_rect_pack.h" "$STB_REPO/stb_rect_pack.h"

echo "stb headers ready at $STB_DIR/"
