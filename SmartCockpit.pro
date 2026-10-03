# 总控：采集 + 推理 + HMI（做法 A：同仓库多 TARGET）
# Qt Creator 打开本文件，或：qmake SmartCockpit.pro && make
TEMPLATE = subdirs
SUBDIRS = dms_capture dms_ai SmartCockpitDMS

dms_capture.file = dms_capture.pro
dms_ai.file = dms_ai.pro
SmartCockpitDMS.file = SmartCockpitDMS.pro
