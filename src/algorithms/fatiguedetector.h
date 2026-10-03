#ifndef FATIGUEDETECTOR_H
#define FATIGUEDETECTOR_H

#include <QString>
#include <opencv2/core.hpp>

// 一次分析的输出结果
struct FatigueResult {
    int level = 0;       // 0 正常 / 1 轻度 / 2 重度
    int headDown = 0;    // 本帧是否低头
    int eyeClosed = 0;   // 本帧是否疑似闭眼
    int persons = 0;     // 本帧人数
};

// 疲劳检测：
//   - 低头：YOLOv8-Pose 关键点几何（需 FATIGUE_HAVE_POSE）
//   - 闭眼：Seeta EyeStateDetector（需 FRAS_HAVE_SEETAFACE + eye_state.csta + pts5）
// 板端由 dms_ai 调用；Linux HMI 不实例化本类。
class FatigueDetector
{
public:
    FatigueDetector();
    ~FatigueDetector();

    // 加载 yolov8_pose.rknn（只调一次）
    bool init(const QString &poseRknnPath);

    // 加载睁闭眼：eye_state.csta + 5 点 landmarker（可用 mask_pts5）
    // 未调用或失败时，闭眼退回弱旁证（ROI），低头仍可用
    bool initEyeState(const QString &eyeStatePath, const QString &landmarker5Path);

    void release();
    bool isReady() const { return m_ready; }
    bool eyeStateReady() const { return m_eyeReady; }

    FatigueResult infer(const cv::Mat &bgr);

    FatigueResult analyzeKeypoints(const float keypoints[17][3], int personCount,
                                   const cv::Mat *bgr = nullptr);

    void reset();

private:
    void updateLevel(int headDown, int eyeClosed);
    // Seeta 官方睁闭眼；失败返回 -1（调用方再考虑旁证）
    int detectEyeClosedSeeta(const cv::Mat &bgr, const float kp[17][3]);

    bool m_ready = false;
    bool m_eyeReady = false;
    void *m_ctx = nullptr;       // rknn_app_context_t*
    void *m_landmarker = nullptr; // seeta::FaceLandmarker*
    void *m_eyeState = nullptr;   // seeta::EyeStateDetector*

    int m_level = 0;
    int m_downCnt = 0;
    int m_eyeCnt = 0;
    int m_normalCnt = 0;
};

#endif // FATIGUEDETECTOR_H
