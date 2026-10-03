#!/usr/bin/env bash
# 粤嵌 RK3568（Buildroot）+ 本机已装交叉工具链
# 用法: source scripts/rk3568/env.gec.sh

export WORK="${WORK:-$HOME/rk3568}"
export TOOLCHAIN="${TOOLCHAIN:-/usr/local/arm-linux/bin}"
export PATH="$TOOLCHAIN:$PATH"

# 有粤嵌 SDK 后改成真实路径，例如:
#   $HOME/rk3568/RK356X_Linux_V1.3.2/buildroot/output/rockchip_rk3568/host/aarch64-buildroot-linux-gnu/sysroot
# 或 SDK 文档里的 sysroot / staging 目录
export SDK="${SDK:-$WORK/sdk}"
export SYSROOT="${SYSROOT:-$SDK/sysroot}"

export DEPS="${DEPS:-$WORK/deps}"
export RK_OPENCV_PREFIX="${RK_OPENCV_PREFIX:-$DEPS/opencv}"
export RK_SEETAFACE_LIB="${RK_SEETAFACE_LIB:-$WORK/seetaface/linux_aarch64}"
# 交叉 Qt 的 qmake（SDK 自带或自行交叉安装后填写）
export RK_QMAKE="${RK_QMAKE:-$DEPS/qt5/bin/qmake}"

mkdir -p "$DEPS" "$WORK/src" "$WORK/build" "$WORK/seetaface/linux_aarch64"

echo "======= GEC RK3568 env ======="
echo "g++: $(command -v aarch64-linux-gnu-g++ || echo MISSING)"
aarch64-linux-gnu-g++ --version 2>/dev/null | head -n1 || true
echo "WORK=$WORK"
echo "SDK=$SDK"
echo "SYSROOT=$SYSROOT"
echo "DEPS=$DEPS"
echo "RK_OPENCV_PREFIX=$RK_OPENCV_PREFIX"
echo "RK_SEETAFACE_LIB=$RK_SEETAFACE_LIB"
echo "RK_QMAKE=$RK_QMAKE"
echo "=============================="
