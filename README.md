# SmartCockpitDMS

> 当前分支说明见文末：**`main` = 多进程 + shm**，**`mono` = 单进程无 shm**。

基于 **RK3568** 的智能座舱驾驶员监测系统（DMS）。

## 功能概览（本分支 `mono`）

| 能力 | 说明 |
|------|------|
| 单进程 | 仅跑 `SmartCockpitDMS`：本进程开相机 + 本地疲劳推理 |
| 无 shm | 不用 `dms_capture` / `dms_ai` / POSIX 共享内存 |
| 疲劳监测 | HMI 内 YOLOv8-Pose（低头）+ Seeta `EyeState`（睁/闭眼） |
| 驾驶员识别 | YuNet 检测 + SFace / RKNN 特征；可选 Seeta 静默活体 |
| 告警 | 疲劳边沿触发语音（ALSA）与可选 CAN |

多进程架构见 `main` 分支。

## 仓库结构

```
SmartCockpitDMS/
├── SmartCockpitDMS.pro       # 单进程 HMI（本分支主要编译目标）
├── dms_capture.pro / dms_ai.pro  # 保留源码，mono 不依赖
├── src/                      # 算法、界面、控制器（监控页自开相机）
├── scripts/rk3568/           # 交叉编译等脚本
├── models/                   # 模型（默认不进 Git，需自行放置）
└── 3rdparty/                 # Seeta / RKNN 头文件等（.so 默认不进 Git）
```

## 数据流（mono）

```
摄像头 → SmartCockpitDMS（OpenCV 采集）
           → 人脸识别确认驾驶员
           → 本进程 Pose + EyeState → 疲劳 UI / 告警
```

## 环境依赖

- 交叉编译主机：Ubuntu + aarch64 工具链 + Qt（如 5.15.2 aarch64）+ OpenCV
- 板端：RK3568、V4L2 摄像头、可选 RKNN Runtime / ALSA
- 第三方：SeetaFace6（Landmarker + EyeState）、RKNN Model Zoo（yolov8_pose 源码，交叉编译 HMI 时链入）

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

## 交叉编译（Ubuntu，mono 只编 HMI）

```bash
cd /path/to/SmartCockpitDMS
export FATIGUE_HAVE_POSE=1
export RKNN_MODEL_ZOO=$HOME/rknn_model_zoo
export RK_ENABLE_SEETAFACE=1

# 只需编 HMI（SmartCockpitDMS.pro 已 DEFINES+=DMS_MONO）
qmake SmartCockpitDMS.pro && make -j$(nproc)
# 或仍可用三进程脚本，板端只跑 ./SmartCockpitDMS 即可
```

产物建议拷到板上 `/root/SmartCockpitDMS/`（含可执行文件、`lib/`、`models/`）。

## 板端运行（mono 单进程）

只需跑 HMI（本进程开相机 + 本地疲劳）：

```sh
cd /root/SmartCockpitDMS
export LD_LIBRARY_PATH="/root/SmartCockpitDMS/lib:${LD_LIBRARY_PATH}"
export QT_QPA_PLATFORM=linuxfb
# 或 ./run.sh
./SmartCockpitDMS
```

**不需要** `dms_capture` / `dms_ai`。

库目录 `lib/` 需包含 OpenCV、RKNN（若启用）、以及 EyeState 相关：

- `libSeetaEyeStateDetector200.so`
- `libSeetaFaceLandmarker600.so`
- `libtennis.so` / `libSeetaAuthorize.so`

## 说明与边界

- **本分支（mono）**：单进程；HMI 自开相机，本地 Pose + EyeState。
- **`main` 分支**：多进程 + shm；疲劳由 `dms_ai` 推送。
- Pose 关键点置信度不能可靠表示「闭眼」，故闭眼使用 Seeta EyeState。
- 活体用于驾驶员确认场景，与疲劳检测解耦。
- `data/attendance.db` 等为历史命名，实际存驾驶员/设置数据。

## 分支对照

| 分支 | 含义 |
|------|------|
| `main` | 多进程：`dms_capture` + `dms_ai` + HMI + shm |
| `mono` | 单进程：仅 HMI（对照实验用） |

```bash
git checkout main   # 多进程
git checkout mono   # 单进程
```

## License

按课程 / 个人项目约定自行补充。第三方（Qt、OpenCV、Seeta、RKNN）遵循其各自许可证。
