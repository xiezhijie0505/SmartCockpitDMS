#!/bin/sh
# Buildroot 板：安装 DMS 开机启动 + 崩溃拉起（无 systemd）
# 板上执行: sh /root/SmartCockpitDMS/systemd/install_sysv_on_board.sh
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
APP_ROOT="$(cd "$DIR/.." && pwd)"

echo "APP_ROOT=$APP_ROOT"

for f in dms_capture dms_ai SmartCockpitDMS; do
  if [ ! -x "$APP_ROOT/$f" ]; then
    echo "缺少可执行文件: $APP_ROOT/$f"
    exit 1
  fi
done

chmod +x "$DIR/S90dms" "$DIR/dms_watchdog.sh"

cp -a "$DIR/S90dms" /etc/init.d/S90dms
chmod +x /etc/init.d/S90dms

# 立刻启动一次
/etc/init.d/S90dms restart

echo "======== status ========"
/etc/init.d/S90dms status || true
echo "日志目录: /tmp/dms_logs"
echo "验收: killall -9 dms_ai; sleep 4; pidof dms_ai"
echo "开机也会自动跑 /etc/init.d/S90dms（S90）"
