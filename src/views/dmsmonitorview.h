#ifndef DMSMONITORVIEW_H
#define DMSMONITORVIEW_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QAtomicInt>
#include <QElapsedTimer>
#include <QColor>
#include <QString>
#include <opencv2/opencv.hpp>

class DriverIdentifyController;
class SystemSettingsController;
class FaceRecognizer;
class FatigueDetector;
class AlsaAlertPlayer;
class FrameShmReader;
class FatigueLevelSocketServer;

class DmsMonitorView : public QWidget
{
    Q_OBJECT

public:
    explicit DmsMonitorView(QWidget *parent = nullptr);
    ~DmsMonitorView();

    void setDriverIdentifyController(DriverIdentifyController *controller);
    void setSettingsController(SystemSettingsController *settingsController);
    void startCamera();
    void stopCamera();
    void setFaceRecognizer(FaceRecognizer *recognizer);
    // Linux 板端传 nullptr（疲劳由 dms_ai 推送）；Windows 可传本地 FatigueDetector 调试
    void setFatigueDetector(FatigueDetector *detector);

private slots:
    void onMatchSuccess(int id, const QString &name, const cv::Mat &faceImage);
    void onMatchHolding(int id, const QString &name);
    void onMatchFailed(const QString &reason);
    void updateFrame();
    void processRecognize();
    void onRemoteFatigueEvent(int level, int headDown, int eyeClosed);

private:
    void setupUI();
    void setupFatigueEventListener();
    void updateStatusLabel(const QString &text, bool isSuccess = true);
    void updateFatigueUi(int level, int headDown, int eyeClosed);

    QLabel *m_videoLabel;
    QLabel *m_levelLabel = nullptr;
    QLabel *m_driverLabel = nullptr;
    QLabel *m_infoLabel;
    QLabel *m_statusLabel;
    QPushButton *m_startBtn;
    QPushButton *m_stopBtn;

    DriverIdentifyController *m_driverController;
    SystemSettingsController *m_settingsController;

    // Linux：读 dms_capture 的 POSIX shm；Windows：走下面 OpenCV m_cap
    FrameShmReader *m_shmReader = nullptr;
    cv::VideoCapture m_cap;
    QTimer *m_timer;
    QTimer *m_recognizeTimer;
    bool m_isCameraRunning;
    int m_cameraIndex;
    cv::Mat m_currentFrame;
    QAtomicInt m_recognizeBusy;

    QElapsedTimer m_fpsTimer;
    int m_fpsFrames;
    double m_displayFps;

    FaceRecognizer *m_faceRecognizer;
    FatigueDetector *m_fatigueDetector = nullptr;
    AlsaAlertPlayer *m_alertPlayer = nullptr;
    int m_lastFatigueLevel = 0;

    bool m_driverLocked = false;
    QString m_driverName;
    int m_driverId = -1;

    cv::Rect m_faceRect;
    float m_faceXf = 0.f; // 浮点平滑，避免整数 EMA 台阶抖动
    float m_faceYf = 0.f;
    float m_faceWf = 0.f;
    float m_faceHf = 0.f;
    bool m_hasFace;
    int m_detectSkip;
    int m_faceMiss = 0; // 连续未检出次数，用于消抖
    QString m_overlayName;
    QColor m_boxColor;
    QElapsedTimer m_matchHighlight;

    // Unix socket 收 dms_ai 的 FatigueEvent（含 level/低头/闭眼）
    FatigueLevelSocketServer *m_fatigueSock = nullptr;
};

#endif // DMSMONITORVIEW_H
