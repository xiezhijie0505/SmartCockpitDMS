#ifndef RKNNENGINE_H
#define RKNNENGINE_H

#include <QString>
#include <QVector>
#include <opencv2/core.hpp>

// 仅在定义了 FRAS_HAVE_RKNN 时真正链接 librknnrt；否则为空壳，方便 Windows 编译。
class RknnEngine
{
public:
    RknnEngine();
    ~RknnEngine();

    RknnEngine(const RknnEngine &) = delete;
    RknnEngine &operator=(const RknnEngine &) = delete;

    bool load(const QString &modelPath);
    void release();
    bool isReady() const { return m_ready; }

    // 输入：NHWC uint8 BGR（与转换时 mean=0/std=255 一致，由 runtime 按图配置处理）
    // 输出：按输出张量拷成 CV_32F 的 Mat 列表
    bool infer(const cv::Mat &nhwcU8, QVector<cv::Mat> *outputs);

private:
    bool m_ready;
    void *m_ctx; // rknn_context，避免头文件污染 Windows
    int m_inputIndex;
    int m_inputWidth;
    int m_inputHeight;
    int m_inputChannels;
    bool m_inputIsNchw;
    bool m_inputIsFloat;
};

#endif // RKNNENGINE_H
