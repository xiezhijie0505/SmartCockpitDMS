#!/usr/bin/env bash
# 检查交叉编译环境是否就绪。用法：source env.sh && bash 01_check_env.sh
set -e
fail=0

check_cmd() {
  local name="$1"
  if command -v "$name" >/dev/null 2>&1; then
    echo "[OK] $name -> $(command -v "$name")"
    "$name" --version 2>/dev/null | head -n1 || true
  else
    echo "[MISSING] 找不到命令: $name"
    fail=1
  fi
}

check_dir() {
  local label="$1" path="$2"
  if [ -d "$path" ]; then
    echo "[OK] $label: $path"
  else
    echo "[MISSING] $label 目录不存在: $path"
    fail=1
  fi
}

check_file_glob() {
  local label="$1" pattern="$2"
  # shellcheck disable=SC2086
  if compgen -G "$pattern" >/dev/null; then
    echo "[OK] $label: $pattern"
    file $pattern 2>/dev/null | head -n3 || true
  else
    echo "[MISSING] $label: $pattern"
    fail=1
  fi
}

echo "======== RK3568 env check ========"
# 工具链名因厂商而异，优先检测常见前缀
if command -v aarch64-linux-gnu-g++ >/dev/null 2>&1; then
  CXX=aarch64-linux-gnu-g++
elif command -v aarch64-buildroot-linux-gnu-g++ >/dev/null 2>&1; then
  CXX=aarch64-buildroot-linux-gnu-g++
elif command -v aarch64-none-linux-gnu-g++ >/dev/null 2>&1; then
  CXX=aarch64-none-linux-gnu-g++
else
  CXX=""
fi

if [ -n "$CXX" ]; then
  echo "[OK] cross g++: $CXX"
  "$CXX" --version | head -n1
else
  echo "[MISSING] 未找到 aarch64-*-g++，请安装交叉工具链或配置 TOOLCHAIN"
  fail=1
fi

check_dir "SYSROOT" "${SYSROOT:-/nonexistent}"
if [ -d "${SYSROOT:-}/usr/include" ]; then
  echo "[OK] SYSROOT/usr/include 存在"
else
  echo "[MISSING] SYSROOT/usr/include 不存在（需要板子 rootfs/sysroot）"
  fail=1
fi

check_dir "OpenCV 安装前缀" "${RK_OPENCV_PREFIX:-/nonexistent}"
check_file_glob "OpenCV 库" "${RK_OPENCV_PREFIX:-/nonexistent}/lib/libopencv_*.so*"

if [ -n "${RK_QMAKE:-}" ] && [ -x "${RK_QMAKE}" ]; then
  echo "[OK] qmake: $RK_QMAKE"
  "$RK_QMAKE" -query QT_INSTALL_PREFIX || true
else
  echo "[MISSING] RK_QMAKE 不可用: ${RK_QMAKE:-未设置}"
  fail=1
fi

check_file_glob "SeetaFace aarch64 .so" "${RK_SEETAFACE_LIB:-/nonexistent}/libSeetaFace*.so*"

echo "======== result ========"
if [ "$fail" -eq 0 ]; then
  echo "环境齐全，可以编译 OpenCV/工程或继续下一步。"
  exit 0
else
  echo "仍有缺失项。把本脚本完整输出发给助手，或按提示补齐文件后再跑。"
  exit 1
fi
