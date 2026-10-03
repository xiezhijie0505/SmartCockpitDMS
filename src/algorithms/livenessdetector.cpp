#include "livenessdetector.h"

#include <QFileInfo>
#include <QDebug>

#ifdef FRAS_HAVE_SEETAFACE
#include <seeta/Common/CStruct.h>
#include <seeta/Common/Struct.h>
#include <vector>
#include <string>
#include <new>

#if defined(Q_OS_WIN)
// Windows MinGW ↔ MSVC DLL C 桥接
extern "C" {
void *sf6_lm_create(void *self, const SeetaModelSetting *s);
void sf6_lm_destroy(void *self);
int sf6_lm_number(const void *self);
void sf6_lm_mark(const void *self, const SeetaImageData *image,
                 const SeetaRect *face, SeetaPointF *points);
void *sf6_fas_create(void *self, const seeta::ModelSetting *s);
void sf6_fas_destroy(void *self);
int sf6_fas_predict(const void *self, const SeetaImageData *image,
                    const SeetaRect *face, const SeetaPointF *points);
int sf6_fas_predict_video(const void *self, const SeetaImageData *image,
                          const SeetaRect *face, const SeetaPointF *points);
void sf6_fas_reset_video(void *self);
void sf6_fas_set_threshold(void *self, float clarity, float reality);
void sf6_fas_get_pre_frame_score(void *self, float *clarity, float *reality);
}
static const size_t kSeetaObjectSize = 16;
#else
// Linux aarch64：官方 C++ API（RK3399/Ubuntu ARM 包可在 RK3568 上尝试）
#include <seeta/FaceLandmarker.h>
#include <seeta/FaceAntiSpoofing.h>
#endif
#endif // FRAS_HAVE_SEETAFACE

LivenessDetector::LivenessDetector()
    : m_landmarker(nullptr)
    , m_fas(nullptr)
    , m_pointCount(0)
    , m_ready(false)
{
}

LivenessDetector::~LivenessDetector()
{
#ifdef FRAS_HAVE_SEETAFACE
#if defined(Q_OS_WIN)
    if (m_fas) {
        sf6_fas_destroy(m_fas);
        m_fas = nullptr;
    }
    if (m_landmarker) {
        sf6_lm_destroy(m_landmarker);
        m_landmarker = nullptr;
    }
#else
    delete static_cast<seeta::FaceAntiSpoofing *>(m_fas);
    m_fas = nullptr;
    delete static_cast<seeta::FaceLandmarker *>(m_landmarker);
    m_landmarker = nullptr;
#endif
#endif
}

bool LivenessDetector::loadModels(const QString &landmarkerModelPath,
                                  const QString &fasFirstModelPath,
                                  const QString &fasSecondModelPath)
{
#ifndef FRAS_HAVE_SEETAFACE
    Q_UNUSED(landmarkerModelPath);
    Q_UNUSED(fasFirstModelPath);
    Q_UNUSED(fasSecondModelPath);
    // Linux 默认未编 SeetaFace，由 MainWindow 统一提示即可
    m_ready = false;
    return false;
#else
    if (!QFileInfo::exists(landmarkerModelPath)) {
        qWarning() << "活体检测：特征点模型不存在：" << landmarkerModelPath;
        return false;
    }
    if (!QFileInfo::exists(fasFirstModelPath)) {
        qWarning() << "活体检测：局部活体模型不存在：" << fasFirstModelPath;
        return false;
    }
    if (!fasSecondModelPath.isEmpty() && !QFileInfo::exists(fasSecondModelPath)) {
        qWarning() << "活体检测：全局活体模型不存在：" << fasSecondModelPath;
        return false;
    }

#if defined(Q_OS_WIN)
    {
        seeta::ModelSetting setting;
        setting.append(landmarkerModelPath.toStdString());
        setting.set_device(seeta::ModelSetting::CPU);

        void *mem = ::operator new(kSeetaObjectSize);
        void *lm = nullptr;
        try {
            lm = sf6_lm_create(mem, &setting);
        } catch (...) {
            qWarning() << "活体检测：特征点模型加载异常";
            lm = nullptr;
        }
        if (!lm) {
            qWarning() << "活体检测：特征点模型加载失败";
            ::operator delete(mem);
            return false;
        }
        m_landmarker = lm;
        m_pointCount = sf6_lm_number(lm);
        qDebug() << "活体检测：特征点模型加载成功，点数 =" << m_pointCount;
    }

    {
        seeta::ModelSetting setting;
        setting.append(fasFirstModelPath.toStdString());
        if (!fasSecondModelPath.isEmpty())
            setting.append(fasSecondModelPath.toStdString());
        setting.set_device(seeta::ModelSetting::CPU);

        void *mem = ::operator new(kSeetaObjectSize);
        void *fas = nullptr;
        try {
            fas = sf6_fas_create(mem, &setting);
        } catch (...) {
            qWarning() << "活体检测：活体模型加载异常";
            fas = nullptr;
        }
        if (!fas) {
            qWarning() << "活体检测：活体模型加载失败";
            sf6_lm_destroy(m_landmarker);
            m_landmarker = nullptr;
            ::operator delete(mem);
            return false;
        }
        m_fas = fas;
        sf6_fas_set_threshold(m_fas, 0.3f, 0.9f);
        qDebug() << "活体检测：活体模型加载成功，模型数 =" << setting.get_model().size();
    }
#else
    try {
        seeta::ModelSetting lmSetting;
        lmSetting.append(landmarkerModelPath.toStdString());
        lmSetting.set_device(seeta::ModelSetting::CPU);
        auto *lm = new seeta::FaceLandmarker(lmSetting);
        m_landmarker = lm;
        m_pointCount = lm->number();
        qDebug() << "活体检测(Linux)：特征点模型加载成功，点数 =" << m_pointCount;

        seeta::ModelSetting fasSetting;
        fasSetting.append(fasFirstModelPath.toStdString());
        if (!fasSecondModelPath.isEmpty())
            fasSetting.append(fasSecondModelPath.toStdString());
        fasSetting.set_device(seeta::ModelSetting::CPU);
        auto *fas = new seeta::FaceAntiSpoofing(fasSetting);
        fas->SetThreshold(0.3f, 0.9f);
        m_fas = fas;
        qDebug() << "活体检测(Linux)：活体模型加载成功";
    } catch (const std::exception &e) {
        qWarning() << "活体检测(Linux)：模型加载异常:" << e.what();
        delete static_cast<seeta::FaceAntiSpoofing *>(m_fas);
        m_fas = nullptr;
        delete static_cast<seeta::FaceLandmarker *>(m_landmarker);
        m_landmarker = nullptr;
        m_ready = false;
        return false;
    } catch (...) {
        qWarning() << "活体检测(Linux)：模型加载未知异常";
        delete static_cast<seeta::FaceAntiSpoofing *>(m_fas);
        m_fas = nullptr;
        delete static_cast<seeta::FaceLandmarker *>(m_landmarker);
        m_landmarker = nullptr;
        m_ready = false;
        return false;
    }
#endif

    m_ready = true;
    qDebug() << "活体检测模块就绪！";
    return true;
#endif
}

LivenessDetector::Status LivenessDetector::checkLiveness(const cv::Mat &frame, const cv::Rect &faceRect)
{
    return detectInternal(frame, faceRect, false);
}

LivenessDetector::Status LivenessDetector::checkLivenessVideo(const cv::Mat &frame, const cv::Rect &faceRect)
{
    return detectInternal(frame, faceRect, true);
}

void LivenessDetector::resetVideo()
{
#ifdef FRAS_HAVE_SEETAFACE
    if (!m_fas)
        return;
#if defined(Q_OS_WIN)
    sf6_fas_reset_video(m_fas);
#else
    static_cast<seeta::FaceAntiSpoofing *>(m_fas)->ResetVideo();
#endif
#else
    Q_UNUSED(0);
#endif
}

void LivenessDetector::getScores(float &clarity, float &reality)
{
    clarity = 0.0f;
    reality = 0.0f;
#ifdef FRAS_HAVE_SEETAFACE
    if (!m_fas)
        return;
#if defined(Q_OS_WIN)
    sf6_fas_get_pre_frame_score(m_fas, &clarity, &reality);
#else
    static_cast<seeta::FaceAntiSpoofing *>(m_fas)->GetPreFrameScore(&clarity, &reality);
#endif
#endif
}

LivenessDetector::Status LivenessDetector::detectInternal(const cv::Mat &frame,
                                                          const cv::Rect &faceRect,
                                                          bool videoMode)
{
#ifndef FRAS_HAVE_SEETAFACE
    Q_UNUSED(frame);
    Q_UNUSED(faceRect);
    Q_UNUSED(videoMode);
    return FUZZY;
#else
    if (!m_ready || !m_landmarker || !m_fas || frame.empty()
        || faceRect.width <= 0 || faceRect.height <= 0)
        return FUZZY;

    cv::Mat bgr;
    if (frame.channels() == 3) {
        bgr = frame;
    } else if (frame.channels() == 4) {
        cv::cvtColor(frame, bgr, cv::COLOR_BGRA2BGR);
    } else if (frame.channels() == 1) {
        cv::cvtColor(frame, bgr, cv::COLOR_GRAY2BGR);
    } else {
        qWarning() << "活体检测：不支持的图像通道数" << frame.channels();
        return FUZZY;
    }

    SeetaImageData image;
    image.width = bgr.cols;
    image.height = bgr.rows;
    image.channels = bgr.channels();
    image.data = bgr.data;

    SeetaRect face;
    face.x = faceRect.x;
    face.y = faceRect.y;
    face.width = faceRect.width;
    face.height = faceRect.height;

    std::vector<SeetaPointF> points(static_cast<size_t>(m_pointCount > 0 ? m_pointCount : 5));

#if defined(Q_OS_WIN)
    sf6_lm_mark(m_landmarker, &image, &face, points.data());
    const int status = videoMode
        ? sf6_fas_predict_video(m_fas, &image, &face, points.data())
        : sf6_fas_predict(m_fas, &image, &face, points.data());
    return static_cast<Status>(status);
#else
    auto *lm = static_cast<seeta::FaceLandmarker *>(m_landmarker);
    auto *fas = static_cast<seeta::FaceAntiSpoofing *>(m_fas);
    lm->mark(image, face, points.data());
    const auto status = videoMode
        ? fas->PredictVideo(image, face, points.data())
        : fas->Predict(image, face, points.data());
    return static_cast<Status>(status);
#endif
#endif
}
