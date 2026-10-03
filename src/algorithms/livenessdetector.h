#ifndef LIVENESSDETECTOR_H
#define LIVENESSDETECTOR_H

#include <opencv2/opencv.hpp>
#include <QString>

// =============================================================================
// SeetaFace6 静默活体检测封装
//
// Windows（MinGW）：官方 DLL 为 MSVC ABI，通过 C 桥接调用。
// Linux aarch64：使用官方 C++ API + linux_aarch64 下的 .so（如 RK3399 ARM 包）。
// 未定义 FRAS_HAVE_SEETAFACE 时：loadModels 失败，退化为纯人脸识别（无活体）。
// =============================================================================

class LivenessDetector
{
public:
    // 活体状态，与 SeetaFace6 FaceAntiSpoofing::Status 取值一致
    enum Status {
        REAL = 0,       // 真实人脸
        SPOOF = 1,      // 攻击人脸（照片/屏幕等假体）
        FUZZY = 2,      // 无法判断（人脸成像质量不好）
        DETECTING = 3   // 正在检测（Video 模式，帧数尚未达到判定要求）
    };

    LivenessDetector();
    ~LivenessDetector();

    // 加载模型
    //   landmarkerModelPath：人脸 5 点特征点模型（face_landmarker_mask_pts5.csta）
    //   fasFirstModelPath   ：局部活体模型（fas_first.csta，必选）
    //   fasSecondModelPath  ：全局活体模型（fas_second.csta，可选；传入可提升精度，
    //                          但会降低检测速度）
    // 返回是否全部加载成功
    bool loadModels(const QString &landmarkerModelPath,
                    const QString &fasFirstModelPath,
                    const QString &fasSecondModelPath = QString());

    // 单帧活体检测（响应快，但精度低于 Video 模式，适合抓拍图片场景）
    Status checkLiveness(const cv::Mat &frame, const cv::Rect &faceRect);

    // 连续视频帧活体检测（推荐，摄像头预览场景）
    // 注意：需要每帧都调用（包括人脸消失的帧），换人/新视频段时需 resetVideo()
    Status checkLivenessVideo(const cv::Mat &frame, const cv::Rect &faceRect);

    // 重置 Video 模式内部状态，开始下一次连续检测（人脸丢失后调用）
    void resetVideo();

    // 获取上一次检测的内部分数（清晰度 clarity / 真实度 reality，仅调试用）
    void getScores(float &clarity, float &reality);

    // 模型是否加载成功（未就绪时应跳过活体拦截，避免影响原识别流程）
    bool isReady() const { return m_ready; }

private:
    // 将 OpenCV Mat + 人脸框转换为 SeetaFace 结构并执行检测
    Status detectInternal(const cv::Mat &frame, const cv::Rect &faceRect, bool videoMode);

    void *m_landmarker;   // seeta::FaceLandmarker* 原始指针（通过 C 桥接创建）
    void *m_fas;          // seeta::FaceAntiSpoofing* 原始指针
    int   m_pointCount;   // 特征点数（pts5 模型为 5）
    bool  m_ready;
};

#endif // LIVENESSDETECTOR_H
