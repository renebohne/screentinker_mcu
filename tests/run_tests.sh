#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=========================================================="
echo " Running ScreenTinker MCU Simulator E2E Test Suite"
echo "=========================================================="

cd "$REPO_ROOT"

# Ensure simulator binary exists
if [ ! -f "screentinker_sim" ]; then
    echo "[Build] screentinker_sim not found. Compiling native simulator..."
    if command -v nix-shell >/dev/null 2>&1; then
        nix-shell --run "pio run -e simulator && cp .pio/build/simulator/program screentinker_sim"
    elif command -v make >/dev/null 2>&1 && [ -f "simulator/Makefile" ]; then
        make -C simulator
    else
        pio run -e simulator
        cp .pio/build/simulator/program screentinker_sim
    fi
fi

echo "[Test] Executing Python test suite against mock server..."
python3 -m unittest discover -s tests -p "test_*.py" -v

echo "=========================================================="
echo " 🎉 All ScreenTinker Simulator Tests Passed Successfully!"
echo "=========================================================="
