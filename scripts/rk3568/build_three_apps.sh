#!/usr/bin/env bash
# 在 Ubuntu 交叉编译三进程：dms_capture / dms_ai / SmartCockpitDMS
# 用法（在 Ubuntu 上）:
#   bash scripts/rk3568/build_three_apps.sh
#   bash scripts/rk3568/build_three_apps.sh /home/china/proj/SmartCockpitDMS
set -euo pipefail

ROOT="${1:-$(cd "$(dirname "$0")/../.." && pwd)}"
BUILD_ROOT="${BUILD_ROOT:-$ROOT/build-rk3568-multi}"
DEPLOY="${DEPLOY:-$HOME/rk-cross/SmartCockpitDMS}"

export PATH="/usr/local/arm-linux/bin:${PATH:-}"
export RK_OPENCV_PREFIX="${RK_OPENCV_PREFIX:-$HOME/rk-cross/deps/opencv}"
export RK_ENABLE_SEETAFACE="${RK_ENABLE_SEETAFACE:-0}"
export FATIGUE_HAVE_POSE="${FATIGUE_HAVE_POSE:-1}"
export RKNN_MODEL_ZOO="${RKNN_MODEL_ZOO:-$HOME/rknn_model_zoo}"
export RK_ENABLE_RKNN="${RK_ENABLE_RKNN:-1}"
export RK_RKNN_LIB="${RK_RKNN_LIB:-$ROOT/3rdparty/rknn/lib/linux_aarch64}"

QMAKE_BIN="${RK_QMAKE:-/opt/qtlib-aarch64-5152/bin/qmake}"

echo "======== build_three_apps ========"
echo "ROOT=$ROOT"
echo "BUILD_ROOT=$BUILD_ROOT"
echo "QMAKE=$QMAKE_BIN"
echo "RK_OPENCV_PREFIX=$RK_OPENCV_PREFIX"
echo "FATIGUE_HAVE_POSE=$FATIGUE_HAVE_POSE"
echo "RKNN_MODEL_ZOO=$RKNN_MODEL_ZOO"
echo "================================="

need() { command -v "$1" >/dev/null || { echo "MISSING: $1"; exit 1; }; }
need aarch64-linux-gnu-g++
need make
if [ ! -x "$QMAKE_BIN" ]; then
  echo "qmake not found: $QMAKE_BIN"
  exit 1
fi
if [ ! -f "$RK_OPENCV_PREFIX/lib/libopencv_core.so" ] && ! ls "$RK_OPENCV_PREFIX/lib"/libopencv_core.so* >/dev/null 2>&1; then
  echo "OpenCV not found under RK_OPENCV_PREFIX=$RK_OPENCV_PREFIX"
  exit 1
fi
if [ "$FATIGUE_HAVE_POSE" = "1" ] && [ ! -f "$RKNN_MODEL_ZOO/examples/yolov8_pose/cpp/postprocess.cc" ]; then
  echo "pose sources not found: $RKNN_MODEL_ZOO/examples/yolov8_pose/cpp/postprocess.cc"
  echo "export RKNN_MODEL_ZOO=... or FATIGUE_HAVE_POSE=0"
  exit 1
fi

build_one() {
  local name="$1"
  local pro="$2"
  local outdir="$BUILD_ROOT/$name"
  mkdir -p "$outdir"
  echo "---- building $name ----"
  (
    cd "$outdir"
    "$QMAKE_BIN" "$ROOT/$pro"
    make -j"$(nproc)"
  )
  local bin
  bin="$(find "$outdir" -maxdepth 2 -type f -executable -name "$name" | head -n1)"
  if [ -z "$bin" ]; then
    echo "binary not found for $name under $outdir"
    exit 1
  fi
  file "$bin"
  echo "OK $bin"
}

rm -rf "$BUILD_ROOT"
mkdir -p "$BUILD_ROOT"

build_one dms_capture dms_capture.pro
build_one dms_ai dms_ai.pro
build_one SmartCockpitDMS SmartCockpitDMS.pro

echo "======== pack deploy ========"
mkdir -p "$DEPLOY/lib" "$DEPLOY/models/rknn" "$DEPLOY/data" "$DEPLOY/assets" \
         "$DEPLOY/plugins/sqldrivers" "$DEPLOY/plugins/platforms"

cp -a "$BUILD_ROOT/dms_capture/dms_capture" "$DEPLOY/" 2>/dev/null \
  || cp -a "$(find "$BUILD_ROOT/dms_capture" -name dms_capture -type f -executable | head -n1)" "$DEPLOY/dms_capture"
cp -a "$BUILD_ROOT/dms_ai/dms_ai" "$DEPLOY/" 2>/dev/null \
  || cp -a "$(find "$BUILD_ROOT/dms_ai" -name dms_ai -type f -executable | head -n1)" "$DEPLOY/dms_ai"
cp -a "$BUILD_ROOT/SmartCockpitDMS/SmartCockpitDMS" "$DEPLOY/" 2>/dev/null \
  || cp -a "$(find "$BUILD_ROOT/SmartCockpitDMS" -name SmartCockpitDMS -type f -executable | head -n1)" "$DEPLOY/SmartCockpitDMS"

cp -a "$RK_OPENCV_PREFIX/lib"/libopencv_*.so* "$DEPLOY/lib/" 2>/dev/null || true

QT_LIBS="$("$QMAKE_BIN" -query QT_INSTALL_LIBS)"
QT_PLUGINS="$("$QMAKE_BIN" -query QT_INSTALL_PLUGINS)"
for n in Qt5Core Qt5Gui Qt5Widgets Qt5Sql Qt5Concurrent Qt5XcbQpa Qt5DBus Qt5Network; do
  cp -a "$QT_LIBS"/lib${n}.so* "$DEPLOY/lib/" 2>/dev/null || true
done
cp -a "$QT_PLUGINS/platforms"/libqxcb.so "$DEPLOY/plugins/platforms/" 2>/dev/null || true
cp -a "$QT_PLUGINS/platforms"/libqlinuxfb.so "$DEPLOY/plugins/platforms/" 2>/dev/null || true
cp -a "$QT_PLUGINS/platforms"/libqeglfs.so "$DEPLOY/plugins/platforms/" 2>/dev/null || true
cp -a "$QT_PLUGINS/sqldrivers"/libqsqlite.so "$DEPLOY/plugins/sqldrivers/" 2>/dev/null || true

if [ -f "$ROOT/3rdparty/rknn/lib/linux_aarch64/librknnrt.so" ]; then
  cp -a "$ROOT/3rdparty/rknn/lib/linux_aarch64/librknnrt.so" "$DEPLOY/lib/"
fi
cp -a "$ROOT/models/rknn/"*.rknn "$DEPLOY/models/rknn/" 2>/dev/null || true
cp -a "$ROOT/models/"*.onnx "$DEPLOY/models/" 2>/dev/null || true
cp -a "$ROOT/assets/"*.wav "$DEPLOY/assets/" 2>/dev/null || true

cat > "$DEPLOY/run_capture.sh" << 'EOF'
#!/bin/sh
DIR=$(cd "$(dirname "$0")" && pwd)
cd "$DIR" || exit 1
export LD_LIBRARY_PATH="$DIR/lib:${LD_LIBRARY_PATH:-}"
exec ./dms_capture "$@"
EOF

cat > "$DEPLOY/run_ai.sh" << 'EOF'
#!/bin/sh
DIR=$(cd "$(dirname "$0")" && pwd)
cd "$DIR" || exit 1
export LD_LIBRARY_PATH="$DIR/lib:${LD_LIBRARY_PATH:-}"
# 默认用部署包内模型；也可: ./run_ai.sh /path/to/yolov8_pose.rknn
if [ -n "$1" ]; then
  exec ./dms_ai "$1"
fi
exec ./dms_ai "$DIR/models/rknn/yolov8_pose.rknn"
EOF

cat > "$DEPLOY/run.sh" << 'EOF'
#!/bin/sh
DIR=$(cd "$(dirname "$0")" && pwd)
cd "$DIR" || exit 1
export LD_LIBRARY_PATH="$DIR/lib:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="$DIR/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$DIR/plugins/platforms"
export QT_QPA_PLATFORM="${FORCE_QT_QPA_PLATFORM:-linuxfb}"
export QT_QPA_FONTDIR="${QT_QPA_FONTDIR:-/usr/share/fonts}"
export TZ="${TZ:-CST-8}"
amixer sset 'Playback Path' SPK >/dev/null 2>&1 || true
unset LC_ALL
export LANG=C
exec ./SmartCockpitDMS "$@"
EOF

chmod +x "$DEPLOY/run.sh" "$DEPLOY/run_capture.sh" "$DEPLOY/run_ai.sh"
chmod +x "$DEPLOY/dms_capture" "$DEPLOY/dms_ai" "$DEPLOY/SmartCockpitDMS" 2>/dev/null || true

echo "======== done ========"
echo "DEPLOY=$DEPLOY"
ls -lh "$DEPLOY/dms_capture" "$DEPLOY/dms_ai" "$DEPLOY/SmartCockpitDMS"
file "$DEPLOY/dms_capture" "$DEPLOY/dms_ai" "$DEPLOY/SmartCockpitDMS"
echo "Board test:"
echo "  ./run_capture.sh"
echo "  ./run_ai.sh"
echo "  ./run.sh"
