#!/usr/bin/env bash
# 从板子打包精简 sysroot（在板子上执行），再 scp 回 Ubuntu。
# 用法（SecureCRT 登录板子后）:
#   bash pack_sysroot_on_board.sh
# 然后在 Ubuntu:
#   scp root@板子IP:/tmp/sysroot-partial.tar.gz ~/rk3568/
#   mkdir -p ~/rk3568/sysroot && tar xzf ~/rk3568/sysroot-partial.tar.gz -C ~/rk3568/sysroot

set -e
OUT=/tmp/sysroot-partial.tar.gz
echo "正在打包到 $OUT ..."
tar czf "$OUT" \
  /usr/include \
  /usr/lib/aarch64-linux-gnu \
  /lib/aarch64-linux-gnu \
  2>/dev/null || tar czf "$OUT" /usr/include /usr/lib /lib

ls -lh "$OUT"
echo "完成。请在 Ubuntu 上 scp 拉取该文件并解压为 SYSROOT。"
