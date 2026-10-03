#include "driveridentifycontroller.h"
#include "algorithms/facerecognizer.h"
#include "algorithms/livenessdetector.h"
#include "models/drivermodel.h"
#include "controllers/systemsettingscontroller.h"

#include <QDebug>
#include <QDateTime>
#include <QMutexLocker>
#include <QThread>
#include <QMetaObject>

DriverIdentifyController::DriverIdentifyController(QObject *parent)
    : QObject(parent)
{
    m_matchTimeoutTimer = new QTimer(this);
    m_matchTimeoutTimer->setSingleShot(true);
    connect(m_matchTimeoutTimer, &QTimer::timeout, this, &DriverIdentifyController::resetMatch);
}

void DriverIdentifyController::setDependencies(FaceRecognizer *faceRecog,
                                               DriverModel *driverModel)
{
    m_faceRecognizer = faceRecog;
    m_driverModel = driverModel;
}

void DriverIdentifyController::processFrame(const cv::Mat &frame)
{
    if (!m_faceRecognizer || !m_faceRecognizer->isReady()) {
        QMetaObject::invokeMethod(this, [this]() {
            emit matchFailed(QStringLiteral("人脸识别模块未初始化"));
        }, Qt::QueuedConnection);
        return;
    }

    bool alreadyMatched = false;
    {
        QMutexLocker lock(&m_matchMutex);
        alreadyMatched = (m_currentMatchedId != -1);
    }

    cv::Rect faceRect;
    cv::Mat feature;
    if (alreadyMatched) {
        faceRect = m_faceRecognizer->detectFace(frame);
        if (faceRect.width <= 0 || faceRect.height <= 0) {
            if (m_livenessDetector && m_livenessDetector->isReady()) {
                m_livenessDetector->resetVideo();
            }
            return;
        }
    } else {
        feature = m_faceRecognizer->extractFeature(frame, false, &faceRect);
        if (feature.empty()) {
            if (m_livenessDetector && m_livenessDetector->isReady()) {
                m_livenessDetector->resetVideo();
            }
            return;
        }
    }

    int livenessCode = static_cast<int>(LivenessDetector::REAL);
    if (!alreadyMatched && m_livenessDetector && m_livenessDetector->isReady()) {
        livenessCode = static_cast<int>(
            m_livenessDetector->checkLivenessVideo(frame, faceRect));
    }

    const cv::Mat frameCopy = frame.clone();
    const cv::Mat featureCopy = feature.empty() ? cv::Mat() : feature.clone();
    if (QThread::currentThread() == thread()) {
        finishProcessFrame(frameCopy, featureCopy, faceRect, alreadyMatched, livenessCode);
    } else {
        QMetaObject::invokeMethod(this, [this, frameCopy, featureCopy, faceRect,
                                         alreadyMatched, livenessCode]() {
            finishProcessFrame(frameCopy, featureCopy, faceRect, alreadyMatched, livenessCode);
        }, Qt::QueuedConnection);
    }
}

void DriverIdentifyController::finishProcessFrame(const cv::Mat &frame, const cv::Mat &feature,
                                                  const cv::Rect &faceRect, bool alreadyMatched,
                                                  int livenessCode)
{
    Q_UNUSED(faceRect);
    const auto liveness = static_cast<LivenessDetector::Status>(livenessCode);
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    switch (liveness) {
    case LivenessDetector::SPOOF:
        if (now - m_lastLivenessWarnMs > 3000) {
            m_lastLivenessWarnMs = now;
            qWarning() << "【活体检测】检测到假体攻击（SPOOF）";
            emit matchFailed(QStringLiteral("检测到照片/屏幕攻击，请真人面对摄像头"));
        }
        resetMatch();
        return;
    case LivenessDetector::FUZZY:
        if (now - m_lastLivenessWarnMs > 3000) {
            m_lastLivenessWarnMs = now;
            emit matchFailed(QStringLiteral("图像不清晰，请正对摄像头"));
        }
        break;
    case LivenessDetector::DETECTING:
        break;
    case LivenessDetector::REAL:
    default:
        break;
    }

    if (alreadyMatched) {
        m_matchTimeoutTimer->start(5000);
        int id = -1;
        QString name;
        {
            QMutexLocker lock(&m_matchMutex);
            id = m_currentMatchedId;
            name = m_currentMatchedName;
        }
        if (id != -1) {
            emit matchHolding(id, name);
        }
        return;
    }

    const int matchedId = findMatchingPerson(feature);
    int prevId = -1;
    {
        QMutexLocker lock(&m_matchMutex);
        prevId = m_currentMatchedId;
    }
    if (matchedId != -1 && matchedId != prevId) {
        DriverInfo info = m_driverModel->getPersonById(matchedId);
        if (info.id != -1) {
            {
                QMutexLocker lock(&m_matchMutex);
                m_currentMatchedId = matchedId;
                m_currentMatchedName = info.name;
            }
            emit matchSuccess(matchedId, info.name, frame);
            m_matchTimeoutTimer->start(5000);
        }
    }
}

int DriverIdentifyController::findMatchingPerson(const cv::Mat &feature)
{
    QList<DriverModel::FeatureMatchInfo> featureList =
        m_driverModel->getAllFeaturesForMatch();
    if (featureList.isEmpty()) {
        qWarning() << "特征库为空，请先在驾驶员档案中录入人脸";
        return -1;
    }

    double bestScore = -1.0;
    for (const auto &item : featureList) {
        cv::Mat dbFeature = FaceRecognizer::byteArrayToMat(item.faceFeature);
        if (dbFeature.empty()) {
            continue;
        }
        const double score = m_faceRecognizer->compareFeature(feature, dbFeature);
        if (score > bestScore) {
            bestScore = score;
        }
        if (score >= 0.363) {
            qInfo() << QStringLiteral("【识别】匹配成功 id=%1 相似度=%2")
                           .arg(item.id).arg(score, 0, 'f', 3);
            return item.id;
        }
    }
    // 未命中不每帧刷屏，仅偶发提示最高分
    static qint64 s_lastMissLog = 0;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - s_lastMissLog > 2000) {
        s_lastMissLog = now;
        qInfo() << QStringLiteral("【识别】未匹配 最高相似度=%1（阈值0.363，库内%2人）")
                       .arg(bestScore, 0, 'f', 3)
                       .arg(featureList.size());
    }
    return -1;
}

void DriverIdentifyController::resetMatch()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this]() { resetMatch(); }, Qt::QueuedConnection);
        return;
    }
    {
        QMutexLocker lock(&m_matchMutex);
        m_currentMatchedId = -1;
        m_currentMatchedName.clear();
    }
    m_matchTimeoutTimer->stop();
}

void DriverIdentifyController::setSettingsController(SystemSettingsController *settingsController)
{
    m_settingsController = settingsController;
}

void DriverIdentifyController::setLivenessDetector(LivenessDetector *detector)
{
    m_livenessDetector = detector;
}
