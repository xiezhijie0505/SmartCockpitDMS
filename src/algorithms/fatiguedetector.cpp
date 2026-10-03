#include "fatiguedetector.h"

#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <algorithm>

#include <QFileInfo>
#include <opencv2/imgproc.hpp>

#if defined(FATIGUE_HAVE_POSE)
#include "yolov8-pose.h"
#include "image_utils.h"
#endif

#if defined(DMS_HAVE_EYE_STATE)
#include <seeta/FaceLandmarker.h>
#include <seeta/EyeStateDetector.h>
#include <seeta/Struct.h>
#endif

namespace {

enum {
    KP_NOSE = 0,
    KP_LEYE = 1,
    KP_REYE = 2,
    KP_LSHOULDER = 5,
    KP_RSHOULDER = 6
};

const int kHoldFrames = 6;
const float kKpConfMin = 0.35f;
const float kHeadDownRatio = 0.35f;

#if defined(FATIGUE_HAVE_POSE)
static int matToImageBuffer(const cv::Mat &bgr, image_buffer_t *image)
{
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    if (!rgb.isContinuous()) {
        rgb = rgb.clone();
    }

    memset(image, 0, sizeof(image_buffer_t));
    image->width = rgb.cols;
    image->height = rgb.rows;
    image->width_stride = rgb.cols;
    image->height_stride = rgb.rows;
    image->format = IMAGE_FORMAT_RGB888;
    image->size = rgb.cols * rgb.rows * 3;
    image->virt_addr = (unsigned char *)malloc(image->size);
    if (!image->virt_addr) {
        return -1;
    }
    memcpy(image->virt_addr, rgb.data, image->size);
    return 0;
}
#endif

#if defined(DMS_HAVE_EYE_STATE)
// 用 pose 眼/鼻估人脸框，供 FaceLandmarker
bool faceRectFromPose(const float kp[17][3], int imgW, int imgH, SeetaRect *out)
{
    const float *nose = kp[KP_NOSE];
    const float *leye = kp[KP_LEYE];
    const float *reye = kp[KP_REYE];
    if (nose[2] < kKpConfMin || leye[2] < 0.25f || reye[2] < 0.25f) {
        return false;
    }
    const double lx = leye[0], ly = leye[1];
    const double rx = reye[0], ry = reye[1];
    const double nx = nose[0], ny = nose[1];
    const double iod = std::hypot(lx - rx, ly - ry);
    if (iod < 8.0) {
        return false;
    }
    const double cx = 0.5 * (lx + rx);
    const double cy = 0.45 * (0.5 * (ly + ry)) + 0.55 * ny;
    const double w = iod * 2.5;
    const double h = iod * 3.1;
    int x = static_cast<int>(cx - w * 0.5);
    int y = static_cast<int>(cy - h * 0.42);
    int wi = static_cast<int>(w);
    int hi = static_cast<int>(h);
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + wi > imgW) wi = imgW - x;
    if (y + hi > imgH) hi = imgH - y;
    if (wi < 24 || hi < 24) {
        return false;
    }
    out->x = x;
    out->y = y;
    out->width = wi;
    out->height = hi;
    return true;
}
#endif

}  // namespace

FatigueDetector::FatigueDetector() = default;

FatigueDetector::~FatigueDetector()
{
    release();
}

void FatigueDetector::release()
{
#if defined(FATIGUE_HAVE_POSE)
    if (m_ctx) {
        auto *ctx = static_cast<rknn_app_context_t *>(m_ctx);
        release_yolov8_pose_model(ctx);
        delete ctx;
        m_ctx = nullptr;
        deinit_post_process();
    }
#endif
#if defined(DMS_HAVE_EYE_STATE)
    delete static_cast<seeta::EyeStateDetector *>(m_eyeState);
    m_eyeState = nullptr;
    delete static_cast<seeta::FaceLandmarker *>(m_landmarker);
    m_landmarker = nullptr;
#endif
    m_eyeReady = false;
    m_ready = false;
}

void FatigueDetector::reset()
{
    m_level = 0;
    m_downCnt = 0;
    m_eyeCnt = 0;
    m_normalCnt = 0;
}

bool FatigueDetector::init(const QString &poseRknnPath)
{
    release();

#if defined(FATIGUE_HAVE_POSE)
    auto *ctx = new rknn_app_context_t();
    memset(ctx, 0, sizeof(*ctx));
    init_post_process();

    if (init_yolov8_pose_model(poseRknnPath.toLocal8Bit().constData(), ctx) != 0) {
        delete ctx;
        deinit_post_process();
        return false;
    }

    m_ctx = ctx;
    m_ready = true;
    reset();
    return true;
#else
    Q_UNUSED(poseRknnPath);
    return false;
#endif
}

bool FatigueDetector::initEyeState(const QString &eyeStatePath, const QString &landmarker5Path)
{
#if !defined(DMS_HAVE_EYE_STATE)
    Q_UNUSED(eyeStatePath);
    Q_UNUSED(landmarker5Path);
    std::fprintf(stderr, "[fatigue] 未定义 DMS_HAVE_EYE_STATE，无法用 eye_state\n");
    m_eyeReady = false;
    return false;
#else
    delete static_cast<seeta::EyeStateDetector *>(m_eyeState);
    m_eyeState = nullptr;
    delete static_cast<seeta::FaceLandmarker *>(m_landmarker);
    m_landmarker = nullptr;
    m_eyeReady = false;

    if (!QFileInfo::exists(eyeStatePath)) {
        std::fprintf(stderr, "[fatigue] 缺少 eye_state：%s\n",
                     qPrintable(eyeStatePath));
        return false;
    }
    if (!QFileInfo::exists(landmarker5Path)) {
        std::fprintf(stderr, "[fatigue] 缺少 5 点 landmarker：%s\n",
                     qPrintable(landmarker5Path));
        return false;
    }

    try {
        seeta::ModelSetting lmSetting;
        lmSetting.append(landmarker5Path.toStdString());
        lmSetting.set_device(seeta::ModelSetting::CPU);
        auto *lm = new seeta::FaceLandmarker(lmSetting);
        if (lm->number() < 5) {
            std::fprintf(stderr, "[fatigue] landmarker 点数=%d，需要 ≥5\n", lm->number());
            delete lm;
            return false;
        }
        m_landmarker = lm;

        seeta::ModelSetting eyeSetting;
        eyeSetting.append(eyeStatePath.toStdString());
        eyeSetting.set_device(seeta::ModelSetting::CPU);
        m_eyeState = new seeta::EyeStateDetector(eyeSetting);
        m_eyeReady = true;
        std::fprintf(stderr, "[fatigue] EyeState 就绪（pts=%d）%s + %s\n",
                     lm->number(),
                     qPrintable(landmarker5Path),
                     qPrintable(eyeStatePath));
        return true;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "[fatigue] EyeState 加载异常: %s\n", e.what());
    } catch (...) {
        std::fprintf(stderr, "[fatigue] EyeState 加载未知异常\n");
    }
    delete static_cast<seeta::EyeStateDetector *>(m_eyeState);
    m_eyeState = nullptr;
    delete static_cast<seeta::FaceLandmarker *>(m_landmarker);
    m_landmarker = nullptr;
    m_eyeReady = false;
    return false;
#endif
}

int FatigueDetector::detectEyeClosedSeeta(const cv::Mat &bgr, const float kp[17][3])
{
#if !defined(DMS_HAVE_EYE_STATE)
    Q_UNUSED(bgr);
    Q_UNUSED(kp);
    return -1;
#else
    if (!m_eyeReady || !m_eyeState || !m_landmarker || bgr.empty()) {
        return -1;
    }

    SeetaRect face{};
    if (!faceRectFromPose(kp, bgr.cols, bgr.rows, &face)) {
        return -1;
    }

    cv::Mat cont = bgr.isContinuous() ? bgr : bgr.clone();
    SeetaImageData image{};
    image.width = cont.cols;
    image.height = cont.rows;
    image.channels = cont.channels();
    image.data = cont.data;

    auto *lm = static_cast<seeta::FaceLandmarker *>(m_landmarker);
    auto *esd = static_cast<seeta::EyeStateDetector *>(m_eyeState);
    std::vector<SeetaPointF> points = lm->mark(image, face);
    if (static_cast<int>(points.size()) < 5) {
        return -1;
    }

    seeta::EyeStateDetector::EYE_STATE left = seeta::EyeStateDetector::EYE_UNKNOWN;
    seeta::EyeStateDetector::EYE_STATE right = seeta::EyeStateDetector::EYE_UNKNOWN;
    esd->Detect(image, points.data(), left, right);

    // 双眼都 CLOSE，或一只 CLOSE 且另一只不是明确 OPEN
    const bool lClose = (left == seeta::EyeStateDetector::EYE_CLOSE);
    const bool rClose = (right == seeta::EyeStateDetector::EYE_CLOSE);
    const bool lOpen = (left == seeta::EyeStateDetector::EYE_OPEN);
    const bool rOpen = (right == seeta::EyeStateDetector::EYE_OPEN);
    if ((lClose && rClose) || (lClose && !rOpen) || (rClose && !lOpen)) {
        return 1;
    }
    return 0;
#endif
}

FatigueResult FatigueDetector::infer(const cv::Mat &bgr)
{
    FatigueResult r;

#if defined(FATIGUE_HAVE_POSE)
    if (!m_ready || !m_ctx || bgr.empty()) {
        return r;
    }

    image_buffer_t src;
    if (matToImageBuffer(bgr, &src) != 0) {
        return r;
    }

    object_detect_result_list od;
    memset(&od, 0, sizeof(od));
    auto *ctx = static_cast<rknn_app_context_t *>(m_ctx);
    int ret = inference_yolov8_pose_model(ctx, &src, &od);
    free(src.virt_addr);

    if (ret != 0) {
        return r;
    }

    if (od.count <= 0) {
        float dummy[17][3] = {{0}};
        return analyzeKeypoints(dummy, 0, &bgr);
    }

    return analyzeKeypoints(od.results[0].keypoints, od.count, &bgr);
#else
    Q_UNUSED(bgr);
    return r;
#endif
}

void FatigueDetector::updateLevel(int headDown, int eyeClosed)
{
    if (headDown) {
        m_downCnt++;
        m_normalCnt = 0;
    } else if (m_downCnt > 0) {
        m_downCnt--;
    }

    if (eyeClosed) {
        m_eyeCnt++;
        m_normalCnt = 0;
    } else if (m_eyeCnt > 0) {
        m_eyeCnt--;
    }

    if (!headDown && !eyeClosed) {
        m_normalCnt++;
    }

    if (m_downCnt >= kHoldFrames * 2 || m_eyeCnt >= kHoldFrames * 2
        || (m_downCnt >= kHoldFrames && m_eyeCnt >= kHoldFrames)) {
        m_level = 2;
    } else if (m_downCnt >= kHoldFrames || m_eyeCnt >= kHoldFrames) {
        m_level = 1;
    } else if (m_normalCnt >= kHoldFrames / 2) {
        m_level = 0;
    }
}

FatigueResult FatigueDetector::analyzeKeypoints(const float kp[17][3], int personCount,
                                                const cv::Mat *bgr)
{
    FatigueResult r;
    r.persons = personCount;

    if (personCount <= 0) {
        m_downCnt = 0;
        m_eyeCnt = 0;
        m_normalCnt++;
        if (m_normalCnt >= kHoldFrames) {
            m_level = 0;
        }
        r.level = m_level;
        return r;
    }

    const float *nose = kp[KP_NOSE];
    const float *lsh = kp[KP_LSHOULDER];
    const float *rsh = kp[KP_RSHOULDER];

    int headDown = 0;
    int eyeClosed = 0;

    if (nose[2] > kKpConfMin && lsh[2] > kKpConfMin && rsh[2] > kKpConfMin) {
        float shoulder_y = 0.5f * (lsh[1] + rsh[1]);
        float shoulder_w = fabsf(rsh[0] - lsh[0]);
        if (shoulder_w < 1.0f) {
            shoulder_w = 1.0f;
        }
        float above = shoulder_y - nose[1];
        if (above < kHeadDownRatio * shoulder_w) {
            headDown = 1;
        }
    }

    // 闭眼优先走 Seeta EyeState（官方模型）
    if (bgr && !bgr->empty()) {
        const int seetaRet = detectEyeClosedSeeta(*bgr, kp);
        if (seetaRet >= 0) {
            eyeClosed = seetaRet;
        }
    }

    updateLevel(headDown, eyeClosed);

    r.headDown = headDown;
    r.eyeClosed = eyeClosed;
    r.level = m_level;
    return r;
}
