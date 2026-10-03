#!/bin/sh
# 单窗口采三进程 CPU%，结束自动打表（不采 RSS）
#
# 用法（板上，三进程已启动）:
#   sh cpu_sample.sh           # 默认每 2 秒，共 30 次（约 1 分钟）
#   sh cpu_sample.sh 2 60      # 每 2 秒，共 60 次
#   OUT=/tmp/dms_cpu.csv sh cpu_sample.sh
#
# 只根据已有 CSV 重打表:
#   sh cpu_sample.sh report /tmp/dms_cpu.csv

INTERVAL="${1:-2}"
COUNT="${2:-30}"
OUT="${OUT:-/tmp/dms_cpu.csv}"

if [ "$1" = "report" ]; then
  OUT="${2:-/tmp/dms_cpu.csv}"
fi

HZ=$(getconf CLK_TCK 2>/dev/null || echo 100)

pid_of() {
  pidof "$1" 2>/dev/null | awk '{print $1; exit}'
}

cpu_ticks() {
  p=$1
  [ -n "$p" ] && [ -r "/proc/$p/stat" ] || { echo 0; return; }
  tr ')' '\n' <"/proc/$p/stat" 2>/dev/null | tail -n 1 | awk '{print $12+$13}'
}

print_report() {
  csv=$1
  [ -f "$csv" ] || { echo "没有文件: $csv"; return 1; }
  echo ""
  echo "========== CPU 汇总表 =========="
  awk -F',' '
    NR==1 { next }
    NF<4 { next }
    {
      n++
      c+=$2; h+=$3; a+=$4
      if (n==1 || $2<cmin) cmin=$2
      if (n==1 || $3<hmin) hmin=$3
      if (n==1 || $4<amin) amin=$4
      if (n==1 || $2>cmax) cmax=$2
      if (n==1 || $3>hmax) hmax=$3
      if (n==1 || $4>amax) amax=$4
    }
    END {
      if (n<1) { print "无数据"; exit 1 }
      printf "| 指标 | dms_capture | SmartCockpitDMS | dms_ai |\n"
      printf "|------|-------------|----------------|--------|\n"
      printf "| CPU 平均 (%%) | %.1f | %.1f | %.1f |\n", c/n, h/n, a/n
      printf "| CPU 最小 (%%) | %.1f | %.1f | %.1f |\n", cmin, hmin, amin
      printf "| CPU 最大 (%%) | %.1f | %.1f | %.1f |\n", cmax, hmax, amax
      printf "\n样本数: %d\n", n
    }
  ' "$csv"
  echo "原始: $csv"
  echo "================================"
}

if [ "$1" = "report" ]; then
  print_report "$OUT"
  exit $?
fi

echo "time_s,capture_cpu,hmi_cpu,ai_cpu" >"$OUT"
echo "CPU 采样: 间隔=${INTERVAL}s 次数=${COUNT} -> $OUT"
echo "保持三进程运行；有人脸时测更有代表性。"

i=0
prev_c=0
prev_h=0
prev_a=0
first=1

while [ "$i" -lt "$COUNT" ]; do
  t=$(date +%s 2>/dev/null || echo "$i")
  pc=$(pid_of dms_capture)
  ph=$(pid_of SmartCockpitDMS)
  pa=$(pid_of dms_ai)

  ct=$(cpu_ticks "$pc")
  ht=$(cpu_ticks "$ph")
  at=$(cpu_ticks "$pa")
  ct=${ct:-0}; ht=${ht:-0}; at=${at:-0}

  if [ "$first" -eq 1 ]; then
    cc=0; hc=0; ac=0
    first=0
  else
    cc=$(awk -v d="$((ct - prev_c))" -v hz="$HZ" -v sec="$INTERVAL" \
      'BEGIN{ if(sec<=0||d<0) d=0; printf "%.1f", 100.0*d/(hz*sec) }')
    hc=$(awk -v d="$((ht - prev_h))" -v hz="$HZ" -v sec="$INTERVAL" \
      'BEGIN{ if(sec<=0||d<0) d=0; printf "%.1f", 100.0*d/(hz*sec) }')
    ac=$(awk -v d="$((at - prev_a))" -v hz="$HZ" -v sec="$INTERVAL" \
      'BEGIN{ if(sec<=0||d<0) d=0; printf "%.1f", 100.0*d/(hz*sec) }')
  fi

  prev_c=$ct; prev_h=$ht; prev_a=$at

  echo "$t,$cc,$hc,$ac" >>"$OUT"
  echo "[$i] capture=${cc}%  hmi=${hc}%  ai=${ac}%"

  i=$((i + 1))
  [ "$i" -lt "$COUNT" ] && sleep "$INTERVAL"
done

print_report "$OUT"
