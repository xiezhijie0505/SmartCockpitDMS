#include "dmsmonitorview.h"
#include "controllers/driveridentifycontroller.h"
#include "controllers/systemsettingscontroller.h"
#include "algorithms/facerecognizer.h"
#include "algorithms/fatiguedetector.h"
#include "audio/alsalertplayer.h"
#include <QMessageBox>
#include <QDebug>
#include <QStyle>
#include <QSizePolicy>
#include <QThread>
#include <QtConcurrent>
#include <QPainter>
#include <QPen>
#include <QCoreApplication>
#include <cmath>
#include <algorithm>
#include "ipc/frame_shm_reader.h"
#include "ipc/fatigue_level_rx.h"
#include "ipc/fatigue_can_tx.h"
// 疲劳主通道用 Unix socket（含低头/闭眼）。勿再订 D-Bus 仅 level 信号，以免冲掉 UI 细节。

namespace {
void applyStatus(QLabel *label, const QString &status)
{
    label->setProperty("status", status);
    label->style()->unpolish(label);
    label->style()->polish(label);
}
}

DmsMonitorView::DmsMonitorView(QWidget *parent)
    : QWidget(parent)
    , m_driverController(nullptr)
    , m_settingsController(nullptr)
    , m_shmReader(new FrameShmReader())
    , m_timer(nullptr)
    , m_recognizeTimer(nullptr)
    , m_isCameraRunning(false)
    , m_cameraIndex(0)
    , m_recognizeBusy(0)
    , m_fpsFrames(0)
    , m_displayFps(0.0)
    , m_faceRecognizer(nullptr)
    , m_fatigueDetector(nullptr)
    , m_hasFace(false)
    , m_detectSkip(0)
    , m_boxColor(0, 220, 0)
{
    setupUI();
    setupFatigueEventListener();
}

DmsMonitorView::~DmsMonitorView()
{
    stopCamera();
    if (m_fatigueSock) {
        m_fatigueSock->stop();
        delete m_fatigueSock;
        m_fatigueSock = nullptr;
    }
    if (m_shmReader) {
        m_shmReader->close();
        delete m_shmReader;
        m_shmReader = nullptr;
    }
}

void DmsMonitorView::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    m_videoLabel = new QLabel(this);
    m_videoLabel->setObjectName("videoLabel");
    m_videoLabel->setMinimumSize(320, 240);
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setText("未启动摄像头");
    m_videoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    mainLayout->addWidget(m_videoLabel, 1);

    m_driverLabel = new QLabel(QStringLiteral("当前驾驶员：未确认（请先在「驾驶员档案」注册并面向摄像头）"), this);
    m_driverLabel->setStyleSheet(QStringLiteral("font-size:16px;"));
    mainLayout->addWidget(m_driverLabel);

    m_levelLabel = new QLabel(QStringLiteral("疲劳等级：—（确认驾驶员后开始监测）"), this);
    m_levelLabel->setStyleSheet(QStringLiteral("font-size:18px;font-weight:bold;"));
    mainLayout->addWidget(m_levelLabel);

    QHBoxLayout *infoLayout = new QHBoxLayout();
    m_infoLabel = new QLabel(QStringLiteral("流程：注册人脸 → 启动监控 → 识别司机 → 疲劳监测"), this);
    applyStatus(m_infoLabel, "info");
    m_statusLabel = new QLabel(QStringLiteral("状态：就绪"), this);
    applyStatus(m_statusLabel, "success");
    infoLayout->addWidget(m_infoLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(infoLayout);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton(QStringLiteral("启动监控"), this);
    m_stopBtn = new QPushButton(QStringLiteral("停止监控"), this);
    m_stopBtn->setProperty("level", "secondary");
    m_stopBtn->setEnabled(false);

    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_stopBtn);
    btnLayout->addStretch();
    mainLayout->addLayout(btnLayout);

    connect(m_startBtn, &QPushButton::clicked, this, &DmsMonitorView::startCamera);
    connect(m_stopBtn, &QPushButton::clicked, this, &DmsMonitorView::stopCamera);

    // S2：疲劳语音告警（aplay + 冷却）
    m_alertPlayer = new AlsaAlertPlayer(this);
    m_alertPlayer->setCooldownMs(10000);
    m_alertPlayer->setWavPath(
        QCoreApplication::applicationDirPath() + QStringLiteral("/assets/alert_fatigue.wav"));
}

void DmsMonitorView::setDriverIdentifyController(DriverIdentifyController *controller)
{
    m_driverController = controller;
    if (m_driverController) {
        connect(m_driverController, &DriverIdentifyController::matchSuccess,
                this, &DmsMonitorView::onMatchSuccess, Qt::QueuedConnection);
        connect(m_driverController, &DriverIdentifyController::matchHolding,
                this, &DmsMonitorView::onMatchHolding, Qt::QueuedConnection);
        connect(m_driverController, &DriverIdentifyController::matchFailed,
                this, &DmsMonitorView::onMatchFailed, Qt::QueuedConnection);
    }
}

void DmsMonitorView::setSettingsController(SystemSettingsController *settingsController)
{
    m_settingsController = settingsController;
    if (m_settingsController) {
        m_cameraIndex = m_settingsController->getCameraIndex();
    }
}

void DmsMonitorView::startCamera()
{
    if (m_isCameraRunning) {
        return;
    }

    if (m_shmReader) {
        m_shmReader->close();
    }
    if (m_cap.isOpened()) {
        m_cap.release();
    }

#ifdef Q_OS_LINUX
    // S4.2：不抢相机，只读 dms_capture 写入的 shm
    if (!m_shmReader || !m_shmReader->open()) {
        QMessageBox::warning(
            this, QStringLiteral("错误"),
            QStringLiteral("无法打开帧共享内存。\n请先启动 dms_capture。"));
        return;
    }
    qInfo() << QStringLiteral("【监控】已连接采集进程画面（共享内存）");
#else
    m_cap.open(m_cameraIndex);
    if (!m_cap.isOpened()) {
        QMessageBox::warning(this, "错误", "无法打开摄像头，请检查摄像头连接");
        return;
    }
    m_cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    m_cap.set(cv::CAP_PROP_FRAME_WIDTH, 320);
    m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, 240);
    m_cap.set(cv::CAP_PROP_FPS, 30);
    m_cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
    qDebug() << "[Camera] OpenCV opened index" << m_cameraIndex;
#endif

    if (!m_timer) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &DmsMonitorView::updateFrame);
    }
    if (!m_recognizeTimer) {
        m_recognizeTimer = new QTimer(this);
        connect(m_recognizeTimer, &QTimer::timeout, this, &DmsMonitorView::processRecognize);
    }
    m_recognizeBusy.storeRelease(0);
    m_fpsFrames = 0;
    m_displayFps = 0.0;
    m_fpsTimer.restart();
    m_hasFace = false;
    m_detectSkip = 0;
    m_faceMiss = 0;
    m_driverLocked = false;
    m_driverId = -1;
    m_driverName.clear();
    m_overlayName.clear();
    m_matchHighlight.invalidate();
    if (m_fatigueDetector) {
        m_fatigueDetector->reset();
    }
    m_timer->start(33);
    // 200ms 调度一次识别，首认更快
    m_recognizeTimer->start(200);

    m_isCameraRunning = true;
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    if (m_driverLabel) {
        m_driverLabel->setText(QStringLiteral("当前驾驶员：识别中…请正对摄像头"));
    }
    m_infoLabel->setText(QStringLiteral("监控已启动：先完成人脸识别，确认驾驶员后再做疲劳监测"));
    if (m_levelLabel) {
        m_levelLabel->setText(QStringLiteral("疲劳等级：等待确认驾驶员…"));
    }
    updateStatusLabel(QStringLiteral("监控运行中"), true);
}

void DmsMonitorView::stopCamera()
{
    if (m_timer) m_timer->stop();
    if (m_recognizeTimer) m_recognizeTimer->stop();

    for (int i = 0; i < 50 && m_recognizeBusy.loadAcquire(); ++i) {
        QThread::msleep(20);
    }

    if (m_shmReader) {
        m_shmReader->close();
    }
    if (m_cap.isOpened()) {
        m_cap.release();
    }

    m_isCameraRunning = false;
    m_currentFrame.release();

    m_videoLabel->clear();
    m_videoLabel->setText(QStringLiteral("摄像头已停止"));
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_driverLocked = false;
    m_driverId = -1;
    m_driverName.clear();
    m_overlayName.clear();
    m_lastFatigueLevel = 0;
    if (m_driverLabel) {
        m_driverLabel->setText(QStringLiteral("当前驾驶员：未确认（请先在「驾驶员档案」注册并面向摄像头）"));
    }
    if (m_levelLabel) {
        m_levelLabel->setText(QStringLiteral("疲劳等级：—（确认驾驶员后开始监测）"));
    }
    if (m_fatigueDetector) {
        m_fatigueDetector->reset();
    }
    m_infoLabel->setText(QStringLiteral("监控已停止"));
    updateStatusLabel(QStringLiteral("已停止"), false);
}

void DmsMonitorView::updateFrame()
{
    cv::Mat frame;

#ifdef Q_OS_LINUX
    if (!m_shmReader) {
        stopCamera();
        return;
    }
    if (!m_shmReader->copyLatest(frame) || frame.empty()) {
        return;   // 本拍没帧，不要 stop
    }
#else
    if (!m_cap.isOpened()) {
        stopCamera();
        return;
    }
    m_cap.grab();
    if (!m_cap.retrieve(frame) || frame.empty()) {
        if (!m_cap.read(frame) || frame.empty()) {
            return;
        }
    }
#endif

    m_currentFrame = frame;

    ++m_fpsFrames;
    const qint64 elapsedMs = m_fpsTimer.elapsed();
    if (elapsedMs >= 1000) {
        m_displayFps = m_fpsFrames * 1000.0 / elapsedMs;
        m_fpsFrames = 0;
        m_fpsTimer.restart();
    }

    cv::Mat show;
    if (frame.cols > 320 || frame.rows > 240) {
        cv::resize(frame, show, cv::Size(320, 240), 0, 0, cv::INTER_NEAREST);
    } else {
        show = frame.clone();
    }

    // 预览隔 2 帧检测；强 EMA + 死区，抑制检测噪声抖框
    ++m_detectSkip;
    if (m_faceRecognizer && m_faceRecognizer->isReady() && (m_detectSkip % 2 == 0)) {
        const cv::Rect r = m_faceRecognizer->detectFace(show);
        if (r.width > 0 && r.height > 0) {
            if (!m_hasFace) {
                m_faceXf = static_cast<float>(r.x);
                m_faceYf = static_cast<float>(r.y);
                m_faceWf = static_cast<float>(r.width);
                m_faceHf = static_cast<float>(r.height);
            } else {
                const float cx = m_faceXf + m_faceWf * 0.5f;
                const float cy = m_faceYf + m_faceHf * 0.5f;
                const float ncx = static_cast<float>(r.x) + static_cast<float>(r.width) * 0.5f;
                const float ncy = static_cast<float>(r.y) + static_cast<float>(r.height) * 0.5f;
                const float dCenter = std::fabs(ncx - cx) + std::fabs(ncy - cy);
                const float dSize = std::fabs(static_cast<float>(r.width) - m_faceWf)
                                    + std::fabs(static_cast<float>(r.height) - m_faceHf);
                // 中心/尺寸变化很小：视为噪声，不更新（死区）
                if (dCenter > 4.f || dSize > 8.f) {
                    // 旧框权重 0.85，新检测 0.15（比原来 2/3 稳很多）
                    constexpr float a = 0.15f;
                    m_faceXf = m_faceXf * (1.f - a) + static_cast<float>(r.x) * a;
                    m_faceYf = m_faceYf * (1.f - a) + static_cast<float>(r.y) * a;
                    m_faceWf = m_faceWf * (1.f - a) + static_cast<float>(r.width) * a;
                    m_faceHf = m_faceHf * (1.f - a) + static_cast<float>(r.height) * a;
                }
            }
            m_faceRect = cv::Rect(static_cast<int>(m_faceXf + 0.5f),
                                  static_cast<int>(m_faceYf + 0.5f),
                                  std::max(1, static_cast<int>(m_faceWf + 0.5f)),
                                  std::max(1, static_cast<int>(m_faceHf + 0.5f)));
            m_hasFace = true;
            m_faceMiss = 0;
        } else if (++m_faceMiss > 10) {
            // 约 10*2 帧仍无脸才隐藏
            m_hasFace = false;
        }
    }

    // 识别出姓名后，框上持续显示（与控制器匹配保持约 5 秒一致）
    const bool named = !m_overlayName.isEmpty()
                       && m_matchHighlight.isValid()
                       && m_matchHighlight.elapsed() < 5500;
    if (!named && m_matchHighlight.isValid() && m_matchHighlight.elapsed() >= 5500) {
        m_overlayName.clear();
        m_matchHighlight.invalidate();
    }

    if (named) {
        m_boxColor = QColor(0, 200, 80);
    } else if (m_hasFace) {
        m_boxColor = QColor(0, 200, 255);
    } else {
        m_boxColor = QColor(0, 220, 0);
    }

    cv::Mat rgb;
    cv::cvtColor(show, rgb, cv::COLOR_BGR2RGB);
    QImage qimg(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
    QImage canvas = qimg.copy();

    QPainter painter(&canvas);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QString fpsText = QStringLiteral("FPS: %1").arg(m_displayFps, 0, 'f', 1);
    painter.setPen(QPen(Qt::black, 3));
    painter.drawText(8, 22, fpsText);
    painter.setPen(QPen(QColor(0, 255, 0), 1));
    painter.drawText(8, 22, fpsText);

    if (m_hasFace) {
        const QRect box(m_faceRect.x, m_faceRect.y, m_faceRect.width, m_faceRect.height);
        painter.setPen(QPen(m_boxColor, named ? 3 : 2));
        painter.drawRect(box);

        // 识别成功显示姓名；仅检测到人脸时显示「检测中」
        const QString caption = named ? m_overlayName : QStringLiteral("检测中");
        QFont font = painter.font();
        font.setPixelSize(18);
        font.setBold(true);
        painter.setFont(font);
        const QFontMetrics fm(font);
        const int tw = fm.horizontalAdvance(caption) + 12;
        const int th = fm.height() + 8;
        int tx = box.x();
        int ty = box.y() - th - 2;
        if (ty < 0) {
            ty = box.y() + 2;
        }
        painter.fillRect(tx, ty, tw, th, QColor(0, 0, 0, 180));
        painter.setPen(Qt::white);
        painter.drawText(tx + 6, ty + fm.ascent() + 4, caption);
    }
    painter.end();

    m_videoLabel->setPixmap(QPixmap::fromImage(canvas).scaled(
        m_videoLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
}

void DmsMonitorView::processRecognize()
{
    if (!m_isCameraRunning || !m_driverController || m_currentFrame.empty()) {
        return;
    }
    if (m_recognizeBusy.loadAcquire()) {
        return;
    }

    const cv::Mat snap = m_currentFrame.clone();
    m_recognizeBusy.storeRelease(1);
    DriverIdentifyController *ctrl = m_driverController;
    QtConcurrent::run([this, ctrl, snap]() {
        if (ctrl) {
            // 统一缩到 320x240，检测/提特征更快
            cv::Mat small;
            if (snap.cols > 320 || snap.rows > 240) {
                cv::resize(snap, small, cv::Size(320, 240), 0, 0, cv::INTER_AREA);
            } else {
                small = snap;
            }
            ctrl->processFrame(small);
        }
        m_recognizeBusy.storeRelease(0);
    });
}

void DmsMonitorView::setFaceRecognizer(FaceRecognizer *recognizer)
{
    m_faceRecognizer = recognizer;
}

void DmsMonitorView::setFatigueDetector(FatigueDetector *detector)
{
    m_fatigueDetector = detector;
    if (m_levelLabel) {
        m_levelLabel->setText(QStringLiteral("疲劳等级：等待 dms_ai 推送…"));
    }
}

void DmsMonitorView::setupFatigueEventListener()
{
#ifdef Q_OS_LINUX
    // 主通道：Unix socket（FatigueEvent = level + 低头 + 闭眼）
    if (!m_fatigueSock) {
        m_fatigueSock = new FatigueLevelSocketServer(this);
    }
    if (m_fatigueSock->start()) {
        connect(m_fatigueSock, &FatigueLevelSocketServer::fatigueReceived,
                this, &DmsMonitorView::onRemoteFatigueEvent, Qt::QueuedConnection);
    }
    // 故意不订 D-Bus FatigueLevelChanged：该信号只有 level，会把低头/闭眼刷成「否」
#endif
}

void DmsMonitorView::onRemoteFatigueEvent(int level, int headDown, int eyeClosed)
{
    updateFatigueUi(level, headDown, eyeClosed);
}

void DmsMonitorView::updateFatigueUi(int level, int headDown, int eyeClosed)
{
    if (!m_levelLabel) {
        return;
    }
    QString levelText = QStringLiteral("正常");
    QString color = QStringLiteral("#2ecc71");
    if (level >= 2) {
        levelText = QStringLiteral("重度疲劳");
        color = QStringLiteral("#e74c3c");
    } else if (level == 1) {
        levelText = QStringLiteral("轻度疲劳");
        color = QStringLiteral("#f39c12");
    }
    m_levelLabel->setText(
        QStringLiteral("疲劳等级：%1（低头:%2 闭眼:%3）")
            .arg(levelText)
            .arg(headDown ? QStringLiteral("是") : QStringLiteral("否"))
            .arg(eyeClosed ? QStringLiteral("是") : QStringLiteral("否")));
    m_levelLabel->setStyleSheet(
        QStringLiteral("font-size:18px;font-weight:bold;color:%1;").arg(color));

    static int s_lastHead = -1;
    static int s_lastEye = -1;
    if (level != m_lastFatigueLevel || headDown != s_lastHead || eyeClosed != s_lastEye) {
        qInfo() << QStringLiteral("【疲劳】等级=%1 低头=%2 闭眼=%3")
                       .arg(level)
                       .arg(headDown ? QStringLiteral("是") : QStringLiteral("否"))
                       .arg(eyeClosed ? QStringLiteral("是") : QStringLiteral("否"));
        s_lastHead = headDown;
        s_lastEye = eyeClosed;
    }

    // S2：正常 → 疲劳 边沿：语音（若喇叭可用）+ CAN 打 STM32 蜂鸣器
    if (m_lastFatigueLevel == 0 && level >= 1) {
        qInfo() << QStringLiteral("【疲劳】触发告警 level=%1").arg(level);
        if (m_alertPlayer) {
            m_alertPlayer->play();
        }
#ifdef Q_OS_LINUX
        // 网卡名按板子改：粤嵌多为 can1；没有则试 can0
        if (!sendFatigueCanToMcu(level, "can1")) {
            qWarning() << QStringLiteral("【疲劳】CAN 发送失败（检查 can1 是否 up、线是否接好）");
        } else {
            qInfo() << QStringLiteral("【疲劳】已发 CAN 0x210 → STM32");
        }
#endif
    } else if (m_lastFatigueLevel >= 1 && level == 0) {
#ifdef Q_OS_LINUX
        sendFatigueCanToMcu(0, "can1");  // 停蜂鸣
#endif
    }
    m_lastFatigueLevel = level;
}

void DmsMonitorView::onMatchSuccess(int id, const QString &name, const cv::Mat &faceImage)
{
    Q_UNUSED(faceImage);

    const bool firstLock = !m_driverLocked;
    m_driverLocked = true;
    m_driverId = id;
    m_driverName = name;
    if (m_driverLabel) {
        m_driverLabel->setText(QStringLiteral("当前驾驶员：%1 (ID:%2)").arg(name).arg(id));
    }
    // Linux：疲劳由 dms_ai 推送，m_fatigueDetector 恒为 nullptr，不能靠它改文案
    if (firstLock && m_levelLabel) {
        if (m_fatigueDetector) {
            m_fatigueDetector->reset();
        }
        m_levelLabel->setText(QStringLiteral("疲劳等级：监测中（等待 dms_ai）…"));
        m_levelLabel->setStyleSheet(QStringLiteral("font-size:18px;font-weight:bold;color:#3498db;"));
    }

    m_infoLabel->setText(QStringLiteral("驾驶员已确认：%1 — 正在疲劳监测").arg(name));
    applyStatus(m_infoLabel, "success");
    updateStatusLabel(QStringLiteral("已确认：%1").arg(name), true);

    m_overlayName = name;
    m_boxColor = QColor(0, 200, 80);
    m_matchHighlight.restart();
}

void DmsMonitorView::onMatchHolding(int id, const QString &name)
{
    if (name.isEmpty()) {
        return;
    }
    // 保持阶段也视为已确认司机（避免闪断）
    if (!m_driverLocked) {
        m_driverLocked = true;
        m_driverId = id;
        m_driverName = name;
        if (m_driverLabel) {
            m_driverLabel->setText(QStringLiteral("当前驾驶员：%1 (ID:%2)").arg(name).arg(id));
        }
        if (m_fatigueDetector) {
            m_fatigueDetector->reset();
        }
        if (m_levelLabel) {
            m_levelLabel->setText(QStringLiteral("疲劳等级：监测中（等待 dms_ai）…"));
            m_levelLabel->setStyleSheet(QStringLiteral("font-size:18px;font-weight:bold;color:#3498db;"));
        }
    }
    m_overlayName = name;
    m_boxColor = QColor(0, 200, 80);
    m_matchHighlight.restart();
}

void DmsMonitorView::onMatchFailed(const QString &reason)
{
    // 已锁定驾驶员后，短暂识别失败不中断疲劳监测
    if (m_driverLocked) {
        updateStatusLabel(reason, false);
        return;
    }
    m_infoLabel->setText(QStringLiteral("当前识别：未识别到已注册驾驶员"));
    applyStatus(m_infoLabel, "error");
    updateStatusLabel(reason, false);

    m_overlayName.clear();
    m_boxColor = QColor(255, 80, 80);
    m_matchHighlight.invalidate();
}


void DmsMonitorView::updateStatusLabel(const QString &text, bool isSuccess)
{
    m_statusLabel->setText(QStringLiteral("状态：") + text);
    applyStatus(m_statusLabel, isSuccess ? "success" : "error");
}
