#!/usr/bin/env bash
# RK3568 环境变量模板：复制为 env.sh 后按实际路径修改，再 source ./env.sh
#   cp 00_env.sample.sh env.sh && nano env.sh && source ./env.sh

export WORK="${WORK:-$HOME/rk3568}"
export SDK="${SDK:-$WORK/sdk}"
export SYSROOT="${SYSROOT:-$SDK/sysroot}"
# 厂商工具链 bin 目录；若用 apt 安装的交叉编译器，可设为 /usr/bin
export TOOLCHAIN="${TOOLCHAIN:-$SDK/toolchain/bin}"
export DEPS="${DEPS:-$WORK/deps}"
export PATH="$TOOLCHAIN:$PATH"

export RK_OPENCV_PREFIX="${RK_OPENCV_PREFIX:-$DEPS/opencv}"
export RK_SEETAFACE_LIB="${RK_SEETAFACE_LIB:-$WORK/seetaface/linux_aarch64}"
# 交叉 Qt 的 qmake；若 SDK 自带 Qt，改成 SDK 里的 qmake 路径
export RK_QMAKE="${RK_QMAKE:-$DEPS/qt5/bin/qmake}"

mkdir -p "$DEPS" "$WORK/src" "$WORK/build" "$WORK/seetaface/linux_aarch64"

echo "WORK=$WORK"
echo "SDK=$SDK"
echo "SYSROOT=$SYSROOT"
echo "TOOLCHAIN=$TOOLCHAIN"
echo "DEPS=$DEPS"
echo "RK_OPENCV_PREFIX=$RK_OPENCV_PREFIX"
echo "RK_SEETAFACE_LIB=$RK_SEETAFACE_LIB"
echo "RK_QMAKE=$RK_QMAKE"
