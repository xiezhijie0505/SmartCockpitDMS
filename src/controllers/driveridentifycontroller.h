#ifndef DRIVERIDENTIFYCONTROLLER_H
#define DRIVERIDENTIFYCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <opencv2/opencv.hpp>

class FaceRecognizer;
class LivenessDetector;
class DriverModel;
class SystemSettingsController;

// 驾驶员身份识别：人脸比对 + 可选活体，供 DMS 监控页锁定驾驶员
class DriverIdentifyController : public QObject
{
    Q_OBJECT

public:
    explicit DriverIdentifyController(QObject *parent = nullptr);

    void setDependencies(FaceRecognizer *faceRecog, DriverModel *driverModel);
    void processFrame(const cv::Mat &frame);
    void resetMatch();
    void setSettingsController(SystemSettingsController *settingsController);
    void setLivenessDetector(LivenessDetector *detector);

signals:
    void matchSuccess(int personnelId, const QString &name, const cv::Mat &faceImage);
    void matchHolding(int personnelId, const QString &name);
    void matchFailed(const QString &reason);

private:
    FaceRecognizer *m_faceRecognizer = nullptr;
    DriverModel *m_driverModel = nullptr;
    SystemSettingsController *m_settingsController = nullptr;
    LivenessDetector *m_livenessDetector = nullptr;

    int m_currentMatchedId = -1;
    QString m_currentMatchedName;
    QTimer *m_matchTimeoutTimer = nullptr;
    mutable QMutex m_matchMutex;
    qint64 m_lastLivenessWarnMs = 0;
    int m_frameSkipCounter = 0;

    int findMatchingPerson(const cv::Mat &feature);
    void finishProcessFrame(const cv::Mat &frame, const cv::Mat &feature,
                            const cv::Rect &faceRect, bool alreadyMatched,
                            int livenessCode);
};

#endif // DRIVERIDENTIFYCONTROLLER_H
