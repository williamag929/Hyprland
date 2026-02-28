#!/usr/bin/env bash
# ══════════════════════════════════════════════════════════════════════
# Spatial OS — Direct test runner (alternative to ctest)
# Usage:
#   ./scripts/run-spatial-tests.sh              # all spatial tests
#   ./scripts/run-spatial-tests.sh SpatialConfig   # only SpatialConfig suite
#   ./scripts/run-spatial-tests.sh SpatialInput    # only SpatialInputHandler suite
# ══════════════════════════════════════════════════════════════════════

set -euo pipefail

BINARY="./build/hyprland_gtests"
FILTER="${1:-Spatial}"   # default: all tests whose suite starts with "Spatial"

if [[ ! -f "$BINARY" ]]; then
    echo "[ERROR] $BINARY not found."
    echo "        Build first: cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j\$(nproc)"
    exit 1
fi

echo "══════════════════════════════════════════════════════"
echo " Running: $BINARY"
echo " Filter:  --gtest_filter=${FILTER}*"
echo "══════════════════════════════════════════════════════"

"$BINARY" \
    --gtest_filter="${FILTER}*" \
    --gtest_color=yes \
    --gtest_print_time=1

echo ""
echo "══════════════════════════════════════════════════════"
echo " Done."
echo "══════════════════════════════════════════════════════"
