# S4.2 采集进程：V4L2 → POSIX shm（独立可执行文件 dms_capture）
# 用法（板端/交叉）：
#   qmake dms_capture.pro && make
#   ./dms_capture

QT += core
CONFIG += console c++11
CONFIG -= app_bundle

TARGET = dms_capture
TEMPLATE = app

INCLUDEPATH += $$PWD/src

HEADERS += \
    src/ipc/frame_shm.h \
    src/capture/v4l2capture.h

SOURCES += \
    apps/dms_capture/main.cpp \
    src/capture/v4l2capture.cpp

win32 {
    # 板端功能；Windows 仅能编过并提示 Linux only（V4L2/shm 在 Q_OS_LINUX 下）
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
            -lopencv_core -lopencv_imgproc -lopencv_imgcodecs \
            -lopencv_videoio \
            -Wl,-rpath,$${RK_OPENCV_PREFIX}/lib
    LIBS += -lrt
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
