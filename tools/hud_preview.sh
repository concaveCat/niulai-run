#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUTPUT_DIR=${1:-/tmp/niulai-hud-preview}
BUILD_DIR=$(mktemp -d /tmp/niulai-hud-preview-build.XXXXXX)
trap 'rm -rf "$BUILD_DIR"' EXIT

mkdir -p "$OUTPUT_DIR"
gcc -std=c11 -Wall -Wextra -Werror \
    -I"$ROOT_DIR/firmware-esp32/main" \
    -o "$BUILD_DIR/hud_preview" \
    "$ROOT_DIR/firmware-esp32/tests/host/hud_preview.c" \
    "$ROOT_DIR/firmware-esp32/main/display_hud_model.c" \
    "$ROOT_DIR/firmware-esp32/main/display_hud_renderer.c"

echo "portrait:"
"$BUILD_DIR/hud_preview" portrait "$OUTPUT_DIR/hud-portrait"
echo "landscape:"
"$BUILD_DIR/hud_preview" landscape "$OUTPUT_DIR/hud-landscape"
echo "PPM previews written to $OUTPUT_DIR"
