QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += core widgets sql concurrent network
# mono：不依赖 D-Bus / Unix socket 收疲劳事件（本进程本地推理）
qtHaveModule(dbus) {
    QT += dbus
    DEFINES += DMS_HAVE_QT_DBUS
}

CONFIG += c++11

# mono 对照分支：单进程（HMI 自采 + 本地疲劳），不依赖 dms_capture/dms_ai/shm
DEFINES += DMS_MONO
message(HMI: DMS_MONO single-process build)

# Product / binary name (same as folder and .pro)
TARGET = SmartCockpitDMS

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += src/main.cpp \
           src/mainwindow.cpp \
           src/audio/alsalertplayer.cpp \
           src/ipc/frame_shm_reader.cpp \
           src/ipc/fatigue_level_rx.cpp \
           src/ipc/fatigue_can_tx.cpp \
           src/algorithms/facerecognizer.cpp \
           src/algorithms/fatiguedetector.cpp \
           src/algorithms/rknnengine.cpp \
           src/algorithms/featurestorage.cpp \
           src/algorithms/livenessdetector.cpp \
           src/controllers/driverarchivecontroller.cpp \
           src/controllers/driveridentifycontroller.cpp \
           src/controllers/systemsettingscontroller.cpp \
           src/models/databasemanager.cpp \
           src/models/drivermodel.cpp \
           src/models/settingsmodel.cpp \
           src/views/dmsmonitorview.cpp \
           src/views/driverarchiveview.cpp \
           src/views/driveradddialog.cpp \
           src/views/facecapturedialog.cpp \
           src/views/systemsettingsview.cpp
HEADERS += src/mainwindow.h \
    src/utils/datapaths.h \
    src/ipc/frame_shm.h \
    src/ipc/frame_shm_reader.h \
    src/ipc/fatigue_ipc.h \
    src/ipc/fatigue_level_rx.h \
    src/ipc/fatigue_can_ipc.h \
    src/ipc/fatigue_can_tx.h \
    src/ipc/dms_ai_dbus.h \
    src/audio/alsalertplayer.h \
    src/algorithms/facerecognizer.h \
    src/algorithms/fatiguedetector.h \
    src/algorithms/rknnengine.h \
    src/algorithms/featurestorage.h \
    src/algorithms/livenessdetector.h \
    src/controllers/driverarchivecontroller.h \
    src/controllers/driveridentifycontroller.h \
    src/controllers/systemsettingscontroller.h \
    src/models/databasemanager.h \
    src/models/drivermodel.h \
    src/models/settingsmodel.h \
    src/views/dmsmonitorview.h \
    src/views/driverarchiveview.h \
    src/views/driveradddialog.h \
    src/views/facecapturedialog.h \
    src/views/systemsettingsview.h
FORMS += mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
INCLUDEPATH += src

# ---------- OpenCV / SeetaFace：按平台分支 ----------
win32 {
    # 本机 OpenCV：请设置环境变量 OPENCV_DIR（不要把个人绝对路径提交进仓库）
    OPENCV_DIR = $$(OPENCV_DIR)
    isEmpty(OPENCV_DIR) {
        warning(OPENCV_DIR not set — set it before Windows build)
    } else {
        INCLUDEPATH += $${OPENCV_DIR}/include
        LIBS += -L$${OPENCV_DIR}/x64/mingw/bin -lopencv_world455
    }

    # SeetaFace6（Windows DLL + MinGW 导入库）
    DEFINES += FRAS_HAVE_SEETAFACE
    INCLUDEPATH += $$PWD/3rdparty/seetaface/include
    LIBS += -L$$PWD/3rdparty/seetaface/lib -lSeetaFaceAntiSpoofingX600 -lSeetaFaceLandmarker600

    SEETAFACE_BIN = $$PWD/3rdparty/seetaface/bin
    CONFIG(debug, debug|release) {
        SEETAFACE_DEST = $$OUT_PWD/debug
    } else {
        SEETAFACE_DEST = $$OUT_PWD/release
    }
    QMAKE_POST_LINK += $$shell_path($$PWD/deploy_seetaface.bat) $$shell_path($$SEETAFACE_DEST)
}

# RK3568 / 通用 Linux aarch64（交叉或板端，无需完整 SDK）：
#   export RK_OPENCV_PREFIX=...
#   export RK_ENABLE_SEETAFACE=1   # 仅当已有 linux_aarch64 的 .so 时打开
unix:!android {
    RK_OPENCV_PREFIX = $$(RK_OPENCV_PREFIX)
    isEmpty(RK_OPENCV_PREFIX) {
        RK_OPENCV_PREFIX = $$(HOME)/rk-cross/deps/opencv
    }
    INCLUDEPATH += $${RK_OPENCV_PREFIX}/include/opencv4
    INCLUDEPATH += $${RK_OPENCV_PREFIX}/include
    LIBS += -L$${RK_OPENCV_PREFIX}/lib \
            -lopencv_core -lopencv_imgproc -lopencv_imgcodecs \
            -lopencv_dnn -lopencv_objdetect -lopencv_videoio \
            -lopencv_calib3d -lopencv_features2d -lopencv_flann \
            -Wl,-rpath,$${RK_OPENCV_PREFIX}/lib
    # POSIX shm_open / mmap（FrameShmReader）
    LIBS += -lrt

    # Linux 使用官方 C++ API + aarch64 .so（默认：库存在则开启；export RK_ENABLE_SEETAFACE=0 可关闭）
    RK_ENABLE_SEETAFACE = $$(RK_ENABLE_SEETAFACE)
    RK_SEETAFACE_LIB = $$(RK_SEETAFACE_LIB)
    isEmpty(RK_SEETAFACE_LIB) {
        RK_SEETAFACE_LIB = $$PWD/3rdparty/seetaface/lib/linux_aarch64
    }
    isEmpty(RK_ENABLE_SEETAFACE) {
        exists($${RK_SEETAFACE_LIB}/libSeetaFaceAntiSpoofingX600.so) {
            RK_ENABLE_SEETAFACE = 1
        } else {
            RK_ENABLE_SEETAFACE = 0
        }
    }
    equals(RK_ENABLE_SEETAFACE, 1) {
        DEFINES += FRAS_HAVE_SEETAFACE
        # 单进程也在 HMI 内做 EyeState 闭眼
        DEFINES += DMS_HAVE_EYE_STATE
        INCLUDEPATH += $$PWD/3rdparty/seetaface/include
        LIBS += -L$${RK_SEETAFACE_LIB} \
                -lSeetaFaceAntiSpoofingX600 -lSeetaFaceLandmarker600 \
                -lSeetaEyeStateDetector200 \
                -ltennis -lSeetaAuthorize \
                -Wl,-rpath,$${RK_SEETAFACE_LIB}
        message(SeetaFace Linux enabled: $${RK_SEETAFACE_LIB})
    } else {
        message(SeetaFace Linux disabled)
    }

    # RKNN Runtime（板端 NPU）。有 aarch64 librknnrt.so 则启用识别加速
    RK_ENABLE_RKNN = $$(RK_ENABLE_RKNN)
    RK_RKNN_LIB = $$(RK_RKNN_LIB)
    isEmpty(RK_RKNN_LIB) {
        RK_RKNN_LIB = $$PWD/3rdparty/rknn/lib/linux_aarch64
    }
    isEmpty(RK_ENABLE_RKNN) {
        exists($${RK_RKNN_LIB}/librknnrt.so) {
            RK_ENABLE_RKNN = 1
        } else {
            RK_ENABLE_RKNN = 0
        }
    }
    equals(RK_ENABLE_RKNN, 1) {
        DEFINES += FRAS_HAVE_RKNN
        INCLUDEPATH += $$PWD/3rdparty/rknn/include
        LIBS += -L$${RK_RKNN_LIB} -lrknnrt -Wl,-rpath,$${RK_RKNN_LIB}
        message(RKNN enabled: $${RK_RKNN_LIB})
    } else {
        message(RKNN disabled)
    }
    # 必须与环境变量、equals 使用同一名字 FATIGUE_HAVE_POSE（不要写成 FATIGQE）
    FATIGUE_HAVE_POSE = $$(FATIGUE_HAVE_POSE)
    RKNN_MODEL_ZOO = $$(RKNN_MODEL_ZOO)
    isEmpty(RKNN_MODEL_ZOO) {
        RKNN_MODEL_ZOO = $$(HOME)/rknn_model_zoo
    }
    POSE_DIR = $${RKNN_MODEL_ZOO}/examples/yolov8_pose/cpp
    equals(FATIGUE_HAVE_POSE, 1) {
    # 再检查 postprocess.cc 是否真的存在，避免路径写错还硬编
    exists($${POSE_DIR}/postprocess.cc) {
        # 告诉 C++ 预处理器：定义宏 FATIGUE_HAVE_POSE
        # fatiguedetector.cpp 里 #if defined(FATIGUE_HAVE_POSE) 才会编译真正的 init/infer
        DEFINES += FATIGUE_HAVE_POSE
        # 头文件搜索路径：yolov8-pose.h、postprocess.h 所在目录
        INCLUDEPATH += $${POSE_DIR}
        # rknpu2 子目录（有的头/源在这里）
        INCLUDEPATH += $${POSE_DIR}/rknpu2
        # zoo 公共工具头文件（image_utils.h 等）
        INCLUDEPATH += $${RKNN_MODEL_ZOO}/utils
        # RKNPU 运行时相关头文件
        INCLUDEPATH += $${RKNN_MODEL_ZOO}/3rdparty/rknpu2/include
        # RGA（image_utils.c 需要 im2d.h）
        INCLUDEPATH += $${RKNN_MODEL_ZOO}/3rdparty/librga/include
        # stb_image（image_utils.c 读写图）
        INCLUDEPATH += $${RKNN_MODEL_ZOO}/3rdparty/stb_image
        # turbojpeg（image_utils.c 需要 turbojpeg.h）
        INCLUDEPATH += $${RKNN_MODEL_ZOO}/3rdparty/jpeg_turbo/include
        # 把 pose 后处理源文件编进本工程
        SOURCES += $${POSE_DIR}/postprocess.cc
        # 把 yolov8-pose 推理封装源文件编进本工程
        SOURCES += $${POSE_DIR}/rknpu2/yolov8-pose.cc
        # 图像缓冲工具（Mat/图 ↔ image_buffer 等）
        SOURCES += $${RKNN_MODEL_ZOO}/utils/image_utils.c
        # 读文件等工具
        SOURCES += $${RKNN_MODEL_ZOO}/utils/file_utils.c
        # 画图工具（pose 依赖里常会链到，即使你不画也可能要编）
        SOURCES += $${RKNN_MODEL_ZOO}/utils/image_drawing.c
        # 链接 librga；pose 还依赖 librknnrt（工程 3rdparty 没有时用 zoo 自带）
        LIBS += -L$${RKNN_MODEL_ZOO}/3rdparty/librga/Linux/aarch64 -lrga
        LIBS += $${RKNN_MODEL_ZOO}/3rdparty/jpeg_turbo/Linux/aarch64/libturbojpeg.a
        !equals(RK_ENABLE_RKNN, 1) {
            LIBS += -L$${RKNN_MODEL_ZOO}/3rdparty/rknpu2/Linux/aarch64 -lrknnrt \
                    -Wl,-rpath,$${RKNN_MODEL_ZOO}/3rdparty/rknpu2/Linux/aarch64
        }
        LIBS += -lpthread -ldl -lm
        # qmake 时在终端打印一行，确认 pose 已打开、路径对不对
        message(FATIGUE_HAVE_POSE on: $${POSE_DIR})
    } else {
        # 开关开了但找不到源码 → 警告，避免静默没编上 pose
        warning(pose not found: $${POSE_DIR})
    }
}
}

RESOURCES += resources.qrc

# S4.2：其它进程工程/入口仅展示；reader 已进本 TARGET 的 SOURCES/HEADERS
# 完整多进程工程请打开 SmartCockpit.pro（subdirs）
# DISTFILES：让 Qt Creator 工程树能看到（不必编译）
DISTFILES += \
    SmartCockpit.pro \
    dms_capture.pro \
    dms_ai.pro \
    apps/dms_capture/main.cpp \
    apps/dms_ai/main.cpp \
    src/ipc/dms_ai_adaptor.h \
    src/ipc/dms_ai_adaptor.cpp \
    systemd/dms-capture.service \
    systemd/dms-hmi.service \
    systemd/dms-ai.service \
    systemd/install_on_board.sh \
    systemd/S90dms \
    systemd/dms_watchdog.sh \
    systemd/install_sysv_on_board.sh

OTHER_FILES += $$DISTFILES
