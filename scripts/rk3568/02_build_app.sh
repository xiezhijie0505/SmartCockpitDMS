#!/usr/bin/env bash
# 在 Ubuntu 上交叉编译本工程（需先 source env.sh 且 01_check_env.sh 通过）
# 用法:
#   source scripts/rk3568/env.sh
#   bash scripts/rk3568/02_build_app.sh /path/to/SmartCockpitDMS

set -e
ROOT="${1:-$(cd "$(dirname "$0")/../.." && pwd)}"
BUILD="$ROOT/build-rk3568"
QMAKE_BIN="${RK_QMAKE:-qmake}"

echo "Project: $ROOT"
echo "qmake:   $QMAKE_BIN"
mkdir -p "$BUILD"
cd "$BUILD"
"$QMAKE_BIN" "$ROOT/SmartCockpitDMS.pro" -spec linux-aarch64-gnu-g++ || \
  "$QMAKE_BIN" "$ROOT/SmartCockpitDMS.pro"
make -j"$(nproc)"

BIN=$(find "$BUILD" -maxdepth 2 -type f -executable -name "SmartCockpitDMS" | head -n1)
echo "Built: $BIN"
file "$BIN"
