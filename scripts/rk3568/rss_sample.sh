#!/bin/sh
# 定时采样三进程 VmRSS，写 CSV，用于判断是否内存泄漏
# 用法（板上）:
#   sh rss_sample.sh              # 默认每 10 秒采一次，共 30 次（约 5 分钟）
#   sh rss_sample.sh 5 60         # 每 5 秒，共 60 次
#   OUT=/tmp/rss.csv sh rss_sample.sh

INTERVAL="${1:-10}"
COUNT="${2:-30}"
OUT="${OUT:-/tmp/dms_rss.csv}"

echo "time_s,dms_capture_kb,SmartCockpitDMS_kb,dms_ai_kb" >"$OUT"
echo "采样: 间隔=${INTERVAL}s 次数=${COUNT} -> $OUT"

i=0
while [ "$i" -lt "$COUNT" ]; do
  t=$(date +%s 2>/dev/null || echo "$i")
  c=0; h=0; a=0
  pc=$(pidof dms_capture 2>/dev/null)
  ph=$(pidof SmartCockpitDMS 2>/dev/null)
  pa=$(pidof dms_ai 2>/dev/null)
  [ -n "$pc" ] && c=$(awk '/VmRSS/{print $2; exit}' /proc/$pc/status 2>/dev/null)
  [ -n "$ph" ] && h=$(awk '/VmRSS/{print $2; exit}' /proc/$ph/status 2>/dev/null)
  [ -n "$pa" ] && a=$(awk '/VmRSS/{print $2; exit}' /proc/$pa/status 2>/dev/null)
  c=${c:-0}; h=${h:-0}; a=${a:-0}
  echo "$t,$c,$h,$a" >>"$OUT"
  echo "[$i] capture=${c}kB hmi=${h}kB ai=${a}kB"
  i=$((i + 1))
  [ "$i" -lt "$COUNT" ] && sleep "$INTERVAL"
done

echo "完成: $OUT"
echo "拉到 Ubuntu 画图: scp root@板IP:$OUT . && python3 plot_rss.py $OUT"
