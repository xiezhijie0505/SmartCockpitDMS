#!/bin/sh
# 守护：每 2 秒检查三进程，挂了就拉起（替代 systemd Restart=on-failure）
APP_ROOT="${APP_ROOT:-/root/SmartCockpitDMS}"
export LD_LIBRARY_PATH="$APP_ROOT/lib:${LD_LIBRARY_PATH:-}"
export QT_PLUGIN_PATH="$APP_ROOT/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$APP_ROOT/plugins/platforms"
export QT_QPA_PLATFORM="${FORCE_QT_QPA_PLATFORM:-linuxfb}"
export QT_QPA_FONTDIR="${QT_QPA_FONTDIR:-/usr/share/fonts}"
export TZ="${TZ:-CST-8}"

LOG_DIR=/tmp/dms_logs
MODEL="$APP_ROOT/models/rknn/yolov8_pose.rknn"
mkdir -p "$LOG_DIR"

# 标记本脚本进程（busybox 无进程改名时，靠这个文件记 pid）
echo $$ >"$LOG_DIR/watchdog.pid"

while true; do
  if ! pidof dms_capture >/dev/null 2>&1; then
    echo "$(date) restart dms_capture" >>"$LOG_DIR/watchdog.log"
    (cd "$APP_ROOT" && ./dms_capture >>"$LOG_DIR/dms_capture.log" 2>&1 &)
    sleep 1
  fi
  if ! pidof SmartCockpitDMS >/dev/null 2>&1; then
    echo "$(date) restart SmartCockpitDMS" >>"$LOG_DIR/watchdog.log"
    (cd "$APP_ROOT" && ./SmartCockpitDMS >>"$LOG_DIR/SmartCockpitDMS.log" 2>&1 &)
    sleep 1
  fi
  if ! pidof dms_ai >/dev/null 2>&1; then
    echo "$(date) restart dms_ai" >>"$LOG_DIR/watchdog.log"
    (cd "$APP_ROOT" && ./dms_ai "$MODEL" >>"$LOG_DIR/dms_ai.log" 2>&1 &)
  fi
  sleep 2
done
