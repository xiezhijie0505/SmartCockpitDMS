#!/usr/bin/env bash
# One-shot cross build OpenCV + app for RK3568 (no full SDK required)
# Usage: bash scripts/rk3568/do_cross_build.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
WORK="${WORK:-$HOME/rk-cross}"
DEPS="${DEPS:-$WORK/deps}"
BUILD="${BUILD:-$WORK/build}"
DEPLOY="${DEPLOY:-$WORK/SmartCockpitDMS}"
OPENCV_VER="${OPENCV_VER:-4.5.5}"
OPENCV_SRC="${OPENCV_SRC:-$WORK/src/opencv-${OPENCV_VER}}"

TOOLCHAIN_BIN="${TOOLCHAIN_BIN:-/usr/local/arm-linux/bin}"
QMAKE_BIN="${QMAKE_BIN:-/opt/qtlib-aarch64-5152/bin/qmake}"
export PATH="$TOOLCHAIN_BIN:$PATH"
export RK_OPENCV_PREFIX="$DEPS/opencv"
export RK_SEETAFACE_LIB="${RK_SEETAFACE_LIB:-$ROOT/3rdparty/seetaface/lib/linux_aarch64}"
# 有 linux_aarch64 .so 时默认开启；可 export RK_ENABLE_SEETAFACE=0 关闭
if [ -z "${RK_ENABLE_SEETAFACE:-}" ]; then
  if [ -f "$RK_SEETAFACE_LIB/libSeetaFaceAntiSpoofingX600.so" ]; then
    export RK_ENABLE_SEETAFACE=1
  else
    export RK_ENABLE_SEETAFACE=0
  fi
fi
echo "RK_ENABLE_SEETAFACE=$RK_ENABLE_SEETAFACE"

echo "======== config ========"
echo "ROOT=$ROOT"
echo "WORK=$WORK"
echo "QMAKE=$QMAKE_BIN"
echo "g++=$(command -v aarch64-linux-gnu-g++)"
echo "========================"

need() { command -v "$1" >/dev/null || { echo "MISSING: $1"; exit 1; }; }
need aarch64-linux-gnu-g++
need aarch64-linux-gnu-gcc
need cmake
need make
need wget
need unzip

if [ ! -x "$QMAKE_BIN" ]; then
  echo "qmake not found: $QMAKE_BIN"
  exit 1
fi
if ! "$QMAKE_BIN" -query QT_INSTALL_PREFIX >/dev/null 2>&1; then
  echo "qmake cannot run (need newer glibc / Ubuntu 24)"
  exit 1
fi

mkdir -p "$DEPS" "$BUILD" "$WORK/src"

if ! ls "$DEPS/opencv/lib"/libopencv_core.so* >/dev/null 2>&1; then
  echo "======== build OpenCV ${OPENCV_VER} (aarch64) ========"
  if [ ! -d "$OPENCV_SRC" ]; then
    if [ -f "$HOME/opencv-${OPENCV_VER}.zip" ]; then
      unzip -q "$HOME/opencv-${OPENCV_VER}.zip" -d "$WORK/src"
    elif [ -f "$HOME/opencv-4.5.5.zip" ]; then
      unzip -q "$HOME/opencv-4.5.5.zip" -d "$WORK/src"
    else
      wget -O "$WORK/src/opencv-${OPENCV_VER}.zip" \
        "https://github.com/opencv/opencv/archive/refs/tags/${OPENCV_VER}.zip"
      unzip -q "$WORK/src/opencv-${OPENCV_VER}.zip" -d "$WORK/src"
    fi
  fi
  rm -rf "$BUILD/opencv"
  mkdir -p "$BUILD/opencv"
  cd "$BUILD/opencv"
  cmake "$OPENCV_SRC" \
    -DCMAKE_SYSTEM_NAME=Linux \
    -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++ \
    -DCMAKE_INSTALL_PREFIX="$DEPS/opencv" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=ON \
    -DBUILD_TESTS=OFF \
    -DBUILD_PERF_TESTS=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DBUILD_opencv_apps=OFF \
    -DBUILD_LIST=core,imgproc,imgcodecs,videoio,dnn,objdetect,highgui \
    -DWITH_QT=OFF \
    -DWITH_GTK=OFF \
    -DWITH_FFMPEG=OFF \
    -DWITH_V4L=ON \
    -DWITH_CUDA=OFF \
    -DOPENCV_GENERATE_PKGCONFIG=ON
  make -j"$(nproc)"
  make install
else
  echo "[OK] OpenCV exists: $DEPS/opencv"
fi

file "$DEPS/opencv/lib"/libopencv_core.so* | head -n2
echo "[INFO] RK_ENABLE_SEETAFACE=$RK_ENABLE_SEETAFACE"

echo "======== build app ========"
APP_BUILD="$ROOT/build-rk3568"
rm -rf "$APP_BUILD"
mkdir -p "$APP_BUILD"
cd "$APP_BUILD"
"$QMAKE_BIN" "$ROOT/SmartCockpitDMS.pro"
make -j"$(nproc)"

BIN="$(find "$APP_BUILD" -maxdepth 2 -type f -executable -name 'SmartCockpitDMS' | head -n1)"
if [ -z "$BIN" ]; then
  echo "binary not found"
  exit 1
fi
file "$BIN"

echo "======== pack deploy ========"
rm -rf "$DEPLOY"
mkdir -p "$DEPLOY/lib" "$DEPLOY/models" "$DEPLOY/data" "$DEPLOY/plugins/sqldrivers" "$DEPLOY/plugins/platforms"
cp -a "$BIN" "$DEPLOY/"
cp -a "$DEPS/opencv/lib"/libopencv_*.so* "$DEPLOY/lib/" || true

QT_LIBS="$("$QMAKE_BIN" -query QT_INSTALL_LIBS)"
QT_PLUGINS="$("$QMAKE_BIN" -query QT_INSTALL_PLUGINS)"
for n in Qt5Core Qt5Gui Qt5Widgets Qt5Sql Qt5Concurrent Qt5XcbQpa Qt5DBus Qt5Network; do
  cp -a "$QT_LIBS"/lib${n}.so* "$DEPLOY/lib/" 2>/dev/null || true
done
cp -a "$QT_PLUGINS/platforms"/libqxcb.so "$DEPLOY/plugins/platforms/" 2>/dev/null || true
cp -a "$QT_PLUGINS/platforms"/libqlinuxfb.so "$DEPLOY/plugins/platforms/" 2>/dev/null || true
cp -a "$QT_PLUGINS/platforms"/libqeglfs.so "$DEPLOY/plugins/platforms/" 2>/dev/null || true
cp -a "$QT_PLUGINS/sqldrivers"/libqsqlite.so "$DEPLOY/plugins/sqldrivers/" 2>/dev/null || true

cp -a "$ROOT/models/"*.onnx "$DEPLOY/models/" 2>/dev/null || true
cp -a "$ROOT/models/"*.csta "$DEPLOY/models/" 2>/dev/null || true
cp -a "$ROOT/3rdparty/seetaface/model/"*.csta "$DEPLOY/models/" 2>/dev/null || true
mkdir -p "$DEPLOY/models/rknn"
cp -a "$ROOT/models/rknn/"*.rknn "$DEPLOY/models/rknn/" 2>/dev/null || true
# SeetaFace aarch64 运行库（活体）
if [ -d "$ROOT/3rdparty/seetaface/lib/linux_aarch64" ]; then
  cp -a "$ROOT/3rdparty/seetaface/lib/linux_aarch64"/libSeetaFaceAntiSpoofingX600.so* "$DEPLOY/lib/" 2>/dev/null || true
  cp -a "$ROOT/3rdparty/seetaface/lib/linux_aarch64"/libSeetaFaceLandmarker600.so* "$DEPLOY/lib/" 2>/dev/null || true
  cp -a "$ROOT/3rdparty/seetaface/lib/linux_aarch64"/libtennis.so* "$DEPLOY/lib/" 2>/dev/null || true
  cp -a "$ROOT/3rdparty/seetaface/lib/linux_aarch64"/libSeetaAuthorize.so* "$DEPLOY/lib/" 2>/dev/null || true
fi
# RKNN Runtime 2.x（覆盖板上旧 1.4.0 时请用部署包内 lib）
if [ -f "$ROOT/3rdparty/rknn/lib/linux_aarch64/librknnrt.so" ]; then
  cp -a "$ROOT/3rdparty/rknn/lib/linux_aarch64/librknnrt.so" "$DEPLOY/lib/"
fi
cp -a "$ROOT/data/attendance.db" "$DEPLOY/data/" 2>/dev/null || true
mkdir -p "$DEPLOY/assets"
cp -a "$ROOT/assets/"*.wav "$DEPLOY/assets/" 2>/dev/null || true

cat > "$DEPLOY/run.sh" << 'EOF'
#!/bin/sh
# Board launch script. Do NOT set LC_ALL=C.UTF-8 (Buildroot often lacks that locale).
DIR=$(cd "$(dirname "$0")" && pwd)
cd "$DIR" || exit 1

export LD_LIBRARY_PATH="$DIR/lib:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="$DIR/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$DIR/plugins/platforms"
export QT_QPA_PLATFORM="${FORCE_QT_QPA_PLATFORM:-linuxfb}"
export QT_QPA_FONTDIR="${QT_QPA_FONTDIR:-/usr/share/fonts}"
export TZ="${TZ:-CST-8}"

# rk809：默认常是 HP，外放需切到 SPK
amixer sset 'Playback Path' SPK >/dev/null 2>&1 || true
amixer sset Master 100% unmute >/dev/null 2>&1 || true

unset LC_ALL
export LANG=C

exec ./SmartCockpitDMS "$@"
EOF
chmod +x "$DEPLOY/run.sh"

echo "======== done ========"
echo "DEPLOY=$DEPLOY"
du -sh "$DEPLOY"
file "$DEPLOY/SmartCockpitDMS"
