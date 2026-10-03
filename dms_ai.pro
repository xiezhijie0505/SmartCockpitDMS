# S5：dms_ai 读 shm + FatigueDetector；有 QtDBus 时再发 FatigueLevelChanged

QT += core
qtHaveModule(dbus) {
    QT += dbus
    DEFINES += DMS_HAVE_QT_DBUS
    message(dms_ai: Qt DBus enabled)
} else {
    warning(dms_ai: Qt DBus missing — fatigue level via Unix socket)
}
CONFIG += console c++11
CONFIG -= app_bundle

TARGET = dms_ai
TEMPLATE = app

INCLUDEPATH += $$PWD/src

HEADERS += \
    src/ipc/frame_shm.h \
    src/ipc/frame_shm_reader.h \
    src/ipc/fatigue_ipc.h \
    src/ipc/fatigue_level_tx.h \
    src/ipc/dms_ai_dbus.h \
    src/algorithms/fatiguedetector.h

SOURCES += \
    apps/dms_ai/main.cpp \
    src/ipc/frame_shm_reader.cpp \
    src/ipc/fatigue_level_tx.cpp \
    src/algorithms/fatiguedetector.cpp

qtHaveModule(dbus) {
    HEADERS += src/ipc/dms_ai_adaptor.h
    SOURCES += src/ipc/dms_ai_adaptor.cpp
}

win32 {
    INCLUDEPATH += D:/time/8.11/opencv/opencv4.5.5-MinGw7.3.0/include
    LIBS += -LD:/time/8.11/opencv/opencv4.5.5-MinGw7.3.0/x64/mingw/bin -lopencv_world455
}

unix:!android {
    RK_OPENCV_PREFIX = $$(RK_OPENCV_PREFIX)
    isEmpty(RK_OPENCV_PREFIX) {
        RK_OPENCV_PREFIX = $$(HOME)/rk-cross/deps/opencv
    }
    INCLUDEPATH += $${RK_OPENCV_PREFIX}/include/opencv4
    INCLUDEPATH += $${RK_OPENCV_PREFIX}/include
    LIBS += -L$${RK_OPENCV_PREFIX}/lib \
            -lopencv_core -lopencv_imgproc \
            -Wl,-rpath,$${RK_OPENCV_PREFIX}/lib
    LIBS += -lrt

    # SeetaFace：EyeState 闭眼 + FaceLandmarker 5 点（默认有 aarch64 .so 则开）
    RK_ENABLE_SEETAFACE = $$(RK_ENABLE_SEETAFACE)
    RK_SEETAFACE_LIB = $$(RK_SEETAFACE_LIB)
    isEmpty(RK_SEETAFACE_LIB) {
        RK_SEETAFACE_LIB = $$PWD/3rdparty/seetaface/lib/linux_aarch64
    }
    isEmpty(RK_ENABLE_SEETAFACE) {
        exists($${RK_SEETAFACE_LIB}/libSeetaEyeStateDetector200.so) {
            RK_ENABLE_SEETAFACE = 1
        } else {
            RK_ENABLE_SEETAFACE = 0
        }
    }
    equals(RK_ENABLE_SEETAFACE, 1) {
        # DMS_HAVE_EYE_STATE：仅 dms_ai 使用 EyeState（HMI 不链此库）
        DEFINES += FRAS_HAVE_SEETAFACE DMS_HAVE_EYE_STATE
        INCLUDEPATH += $$PWD/3rdparty/seetaface/include
        LIBS += -L$${RK_SEETAFACE_LIB} \
                -lSeetaEyeStateDetector200 -lSeetaFaceLandmarker600 \
                -ltennis -lSeetaAuthorize \
                -Wl,-rpath,$${RK_SEETAFACE_LIB}
        message(dms_ai SeetaFace EyeState enabled: $${RK_SEETAFACE_LIB})
    } else {
        warning(dms_ai SeetaFace disabled — 闭眼无 EyeState)
    }

    # --- 与 SmartCockpitDMS.pro 对齐：RKNN + yolov8_pose ---
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
        message(dms_ai RKNN enabled: $${RK_RKNN_LIB})
    } else {
        message(dms_ai RKNN disabled)
    }

    FATIGUE_HAVE_POSE = $$(FATIGUE_HAVE_POSE)
    RKNN_MODEL_ZOO = $$(RKNN_MODEL_ZOO)
    isEmpty(RKNN_MODEL_ZOO) {
        RKNN_MODEL_ZOO = $$(HOME)/rknn_model_zoo
    }
    POSE_DIR = $${RKNN_MODEL_ZOO}/examples/yolov8_pose/cpp
    equals(FATIGUE_HAVE_POSE, 1) {
    exists($${POSE_DIR}/postprocess.cc) {
        DEFINES += FATIGUE_HAVE_POSE
        INCLUDEPATH += $${POSE_DIR}
        INCLUDEPATH += $${POSE_DIR}/rknpu2
        INCLUDEPATH += $${RKNN_MODEL_ZOO}/utils
        INCLUDEPATH += $${RKNN_MODEL_ZOO}/3rdparty/rknpu2/include
        INCLUDEPATH += $${RKNN_MODEL_ZOO}/3rdparty/librga/include
        INCLUDEPATH += $${RKNN_MODEL_ZOO}/3rdparty/stb_image
        INCLUDEPATH += $${RKNN_MODEL_ZOO}/3rdparty/jpeg_turbo/include
        SOURCES += $${POSE_DIR}/postprocess.cc
        SOURCES += $${POSE_DIR}/rknpu2/yolov8-pose.cc
        SOURCES += $${RKNN_MODEL_ZOO}/utils/image_utils.c
        SOURCES += $${RKNN_MODEL_ZOO}/utils/file_utils.c
        SOURCES += $${RKNN_MODEL_ZOO}/utils/image_drawing.c
        LIBS += -L$${RKNN_MODEL_ZOO}/3rdparty/librga/Linux/aarch64 -lrga
        LIBS += $${RKNN_MODEL_ZOO}/3rdparty/jpeg_turbo/Linux/aarch64/libturbojpeg.a
        !equals(RK_ENABLE_RKNN, 1) {
            LIBS += -L$${RKNN_MODEL_ZOO}/3rdparty/rknpu2/Linux/aarch64 -lrknnrt \
                    -Wl,-rpath,$${RKNN_MODEL_ZOO}/3rdparty/rknpu2/Linux/aarch64
        }
        LIBS += -lpthread -ldl -lm
        message(dms_ai FATIGUE_HAVE_POSE on: $${POSE_DIR})
    } else {
        warning(dms_ai pose not found: $${POSE_DIR})
    }
    }
}

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
