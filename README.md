# SmartCockpitDMS

基于 **RK3568** 的智能座舱驾驶员监测系统（DMS）：摄像头采集 → 共享内存传帧 → AI 疲劳推理 → HMI 预览与驾驶员识别。

## 功能概览

| 能力 | 说明 |
|------|------|
| 多进程架构 | `dms_capture` / `dms_ai` / `SmartCockpitDMS` 分离，崩溃可独立拉起 |
| 帧传输 | POSIX 共享内存 + seqlock（最新帧，允许丢中间帧） |
| 疲劳监测 | YOLOv8-Pose（低头）+ Seeta `EyeState`（睁/闭眼）；等级经 Unix socket 推 HMI |
| 驾驶员识别 | YuNet 检测 + SFace / RKNN 特征；可选 Seeta 静默活体 |
| 告警 | 疲劳边沿触发语音（ALSA）与可选 CAN |

## 仓库结构

```
SmartCockpitDMS/
├── SmartCockpit.pro          # 三进程总工程（subdirs）
├── dms_capture.pro           # 采集进程
├── dms_ai.pro                # 推理进程
├── SmartCockpitDMS.pro       # HMI
├── apps/                     # dms_capture / dms_ai 入口
├── src/                      # 公共源码（IPC、算法、界面、控制器）
├── scripts/rk3568/           # 交叉编译等脚本
├── systemd/                  # 开机/看门狗脚本
├── models/                   # 模型（默认不进 Git，需自行放置）
└── 3rdparty/                 # Seeta / RKNN 头文件等（.so 默认不进 Git）
```

## 进程与数据流

```
摄像头(V4L2)
    → dms_capture 写入 /dms_frame_shm
        → dms_ai 读帧：Pose 低头 + EyeState 闭眼 → Unix socket 推送等级
        → SmartCockpitDMS 读帧预览 + 人脸识别；收 socket 更新疲劳 UI
```

启动顺序建议：`dms_capture` → `dms_ai` → `SmartCockpitDMS`。

## 环境依赖

- 交叉编译主机：Ubuntu + aarch64 工具链 + Qt（如 5.15.2 aarch64）+ OpenCV
- 板端：RK3568、V4L2 摄像头、可选 RKNN Runtime / ALSA
- 第三方：SeetaFace6（Landmarker + EyeState）、RKNN Model Zoo（yolov8_pose 源码，编译 `dms_ai` 时用）

## 模型文件（需自行下载，默认不提交）

放到 `models/`（或板上 `/root/SmartCockpitDMS/models/`）：

| 文件 | 用途 |
|------|------|
| `rknn/yolov8_pose.rknn` | 姿态 / 低头 |
| `eye_state.csta` | Seeta 睁闭眼 |
| `face_landmarker_mask_pts5.csta` | 5 点关键点（EyeState 输入） |
| `face_detection_yunet_*.onnx` | 人脸检测（HMI） |
| `rknn/face_recognition_sface_*.rknn` 或对应 onnx | 人脸识别 |
| `fas_first.csta` 等 | 可选活体 |

Seeta 模型包（含 `eye_state.csta`、`pts5` 等）见官方说明：  
https://github.com/seetafaceengine/SeetaFace6

## 交叉编译（Ubuntu）

```bash
cd /path/to/SmartCockpitDMS
export FATIGUE_HAVE_POSE=1
export RKNN_MODEL_ZOO=$HOME/rknn_model_zoo
export RK_ENABLE_SEETAFACE=1

bash scripts/rk3568/build_three_apps.sh
```

产物默认在 `~/rk-cross/SmartCockpitDMS/`（含可执行文件、`lib/`、`run_*.sh`）。

## 板端运行

```sh
cd /root/SmartCockpitDMS
export LD_LIBRARY_PATH="/root/SmartCockpitDMS/lib:${LD_LIBRARY_PATH}"

# 部署前请先停止旧进程 / 看门狗，再 scp 新文件
./run_capture.sh &
sleep 1
./dms_ai models/rknn/yolov8_pose.rknn &
sleep 1
./run.sh &

pidof dms_capture dms_ai SmartCockpitDMS
```

库目录 `lib/` 需包含 OpenCV、以及 EyeState 相关：

- `libSeetaEyeStateDetector200.so`
- `libSeetaFaceLandmarker600.so`
- `libtennis.so` / `libSeetaAuthorize.so`

## 说明与边界

- HMI（Linux）不本地跑 Pose；疲劳等级以 `dms_ai` 推送为准。
- Pose 关键点置信度不能可靠表示「闭眼」，故闭眼使用 Seeta EyeState，而非单纯调 Pose 阈值。
- 活体用于驾驶员确认场景，与疲劳检测解耦。
- `data/attendance.db` 等为历史命名，实际存驾驶员/设置数据。

## License

按课程 / 个人项目约定自行补充。第三方（Qt、OpenCV、Seeta、RKNN）遵循其各自许可证。
