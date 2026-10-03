// dms_ai：读 shm → FatigueDetector(RKNN) → 通知 HMI
// 主通道：Unix socket（FatigueEvent，含低头/闭眼）
// 可选：D-Bus 只发 level，供外部工具；HMI 以 socket 为准，勿双订以免 UI 被刷掉
// argv[1]：pose 模型路径；默认 models/rknn/yolov8_pose.rknn
// 主循环每 5 帧推理一次，降低 NPU/CPU 占用
#include "ipc/frame_shm_reader.h"
#include "ipc/fatigue_level_tx.h"
#include "algorithms/fatiguedetector.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

#include <QCoreApplication>
#include <QString>
#include <QtGlobal>

#ifdef DMS_HAVE_QT_DBUS
#include "ipc/dms_ai_dbus.h"
#include "ipc/dms_ai_adaptor.h"
#include <QDBusConnection>
#include <QDBusError>
#include <QEventLoop>
#include <QObject>
#endif

#ifdef Q_OS_LINUX
#include <csignal>
#endif

namespace {

#ifdef Q_OS_LINUX
std::atomic_bool g_running{true};

void onSignal(int)
{
    g_running.store(false);
}
#endif

} // namespace

int main(int argc, char *argv[])
{
#ifndef Q_OS_LINUX
    (void)argc;
    (void)argv;
    std::fprintf(stderr, "dms_ai is for Linux/RK board only.\n");
    return 1;
#else
    QCoreApplication app(argc, argv);

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

#ifdef DMS_HAVE_QT_DBUS
    QObject dbusParent;
    DmsAiAdaptor adaptor(&dbusParent);

    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        std::fprintf(stderr, "[dms_ai] D-Bus session bus 连不上（仍可用 Unix socket）\n");
    } else if (!bus.registerService(QLatin1String(kDmsAiDbusService))) {
        std::fprintf(stderr, "[dms_ai] registerService failed: %s\n",
                     qPrintable(bus.lastError().message()));
    } else if (!bus.registerObject(QLatin1String(kDmsAiDbusPath), &dbusParent)) {
        std::fprintf(stderr, "[dms_ai] registerObject failed: %s\n",
                     qPrintable(bus.lastError().message()));
    } else {
        std::printf("[dms_ai] D-Bus ready %s %s\n",
                    kDmsAiDbusService, kDmsAiDbusPath);
    }
#else
    std::fprintf(stderr,
                 "【AI】使用 Unix 套接字推送疲劳事件 (/tmp/dms_fatigue.sock)\n");
#endif

    FrameShmReader reader;
    if (!reader.open()) {
        std::fprintf(stderr,
                     "【AI】打不开帧共享内存，请先启动 dms_capture\n");
        return 1;
    }
    std::printf("【AI】已连接采集画面，Ctrl+C 退出\n");

    FatigueDetector detector;
    QString posePath = QStringLiteral("models/rknn/yolov8_pose.rknn");
    if (argc >= 2 && argv[1] && argv[1][0] != '\0') {
        posePath = QString::fromLocal8Bit(argv[1]);
    }
    if (!detector.init(posePath)) {
        std::fprintf(stderr, "【AI】疲劳模型加载失败：%s\n",
                     qPrintable(posePath));
        reader.close();
        return 1;
    }
    std::printf("【AI】Pose 就绪：%s\n", qPrintable(posePath));

    // 闭眼：Seeta EyeState（需 eye_state.csta + 5 点 landmarker）
    {
        QString eyePath = QStringLiteral("models/eye_state.csta");
        QString lm5Path = QStringLiteral("models/face_landmarker_mask_pts5.csta");
        if (argc >= 3 && argv[2] && argv[2][0] != '\0') {
            eyePath = QString::fromLocal8Bit(argv[2]);
        }
        if (argc >= 4 && argv[3] && argv[3][0] != '\0') {
            lm5Path = QString::fromLocal8Bit(argv[3]);
        }
        if (detector.initEyeState(eyePath, lm5Path)) {
            std::printf("【AI】EyeState 闭眼检测就绪\n");
        } else {
            std::fprintf(stderr,
                         "【AI】EyeState 未就绪（仍可低头）。请确认板上有：\n"
                         "  %s\n  %s\n",
                         qPrintable(eyePath), qPrintable(lm5Path));
        }
    }

    cv::Mat frame;
    int frames = 0;
    int skip = 0;
    int lastLevel = -1;
    int lastHead = -1;
    int lastEye = -1;
    auto t0 = std::chrono::steady_clock::now();

    while (g_running.load()) {
        if (!reader.copyLatest(frame)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
#ifdef DMS_HAVE_QT_DBUS
            app.processEvents(QEventLoop::AllEvents, 10);
#endif
            continue;
        }

        if ((++skip % 5) != 0) {
            // 降频：约保留 1/5 帧做 pose，预览仍由 capture 全速写 shm
#ifdef DMS_HAVE_QT_DBUS
            app.processEvents(QEventLoop::AllEvents, 10);
#endif
            continue;
        }

        const FatigueResult r = detector.infer(frame);

        // level 或低头/闭眼任一变化都推给 HMI（否则 level 一直 0 时界面完全不更新）
        if (r.level != lastLevel || r.headDown != lastHead || r.eyeClosed != lastEye) {
            std::printf("【AI】等级=%d 低头=%s 闭眼=%s\n",
                        r.level,
                        r.headDown ? "是" : "否",
                        r.eyeClosed ? "是" : "否");
            if (!sendFatigueEventToHmi(r.level, r.headDown, r.eyeClosed)) {
                std::fprintf(stderr,
                             "【AI】推送失败（请先启动 HMI 监听）\n");
            }
#ifdef DMS_HAVE_QT_DBUS
            // 可选旁路：仅 level；HMI 不订阅此信号
            emit adaptor.FatigueLevelChanged(r.level);
#endif
            lastLevel = r.level;
            lastHead = r.headDown;
            lastEye = r.eyeClosed;
        }

        ++frames;
        const auto now = std::chrono::steady_clock::now();
        const double sec = std::chrono::duration<double>(now - t0).count();
        if (sec >= 5.0) {
            std::fprintf(stderr, "【AI】推理约 %.1f 次/秒，当前等级=%d\n",
                         frames / sec, lastLevel < 0 ? 0 : lastLevel);
            frames = 0;
            t0 = now;
        }

#ifdef DMS_HAVE_QT_DBUS
        app.processEvents(QEventLoop::AllEvents, 10);
#endif
    }

    reader.close();
    std::printf("【AI】已停止\n");
    return 0;
#endif
}
