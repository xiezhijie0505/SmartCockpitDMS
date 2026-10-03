#!/bin/sh
# 在粤嵌板子 SecureCRT 里执行，把完整输出发回助手
#   sh /tmp/probe_board.sh   或直接复制粘贴下面命令

echo "===== OS ====="
cat /etc/os-release 2>/dev/null
uname -a

echo "===== Qt ====="
ls /usr/lib/libQt5* 2>/dev/null | head
ls /usr/lib/qt* 2>/dev/null | head
find /usr -name "libQt5Widgets.so*" 2>/dev/null | head
which qmake 2>/dev/null

echo "===== OpenCV ====="
ls /usr/lib/libopencv* 2>/dev/null | head
find /usr -name "libopencv_core.so*" 2>/dev/null | head

echo "===== video / display ====="
ls /dev/video* 2>/dev/null
echo "DISPLAY=$DISPLAY"
ls /dev/fb* 2>/dev/null

echo "===== disk ====="
df -h /
