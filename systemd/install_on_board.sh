#!/bin/sh
# 在 RK 板上执行：安装并启用 DMS 三进程 systemd 服务
#   sh /root/SmartCockpitDMS/systemd/install_on_board.sh
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

# unit 里写的是 /root/SmartCockpitDMS；若部署目录不同请先改三个 .service
cp -a "$DIR/dms-capture.service" /etc/systemd/system/
cp -a "$DIR/dms-hmi.service" /etc/systemd/system/
cp -a "$DIR/dms-ai.service" /etc/systemd/system/

systemctl daemon-reload
systemctl enable dms-capture.service dms-hmi.service dms-ai.service
systemctl restart dms-capture.service
sleep 1
systemctl restart dms-hmi.service
sleep 1
systemctl restart dms-ai.service

echo "======== status ========"
systemctl --no-pager --full status dms-capture.service dms-hmi.service dms-ai.service || true
echo "OK. 试: kill -9 \$(pidof dms_ai); sleep 3; systemctl status dms-ai"
