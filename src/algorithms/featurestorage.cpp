#include "featurestorage.h"
#include <QDebug>

QByteArray FeatureStorage::matToBytes(const cv::Mat &feature)
{
    if (feature.empty()) {
        qDebug() << "特征矩阵为空，无法转换";
        return QByteArray();
    }

    // 计算总字节数
    size_t totalBytes = feature.total() * feature.elemSize();
    QByteArray byteArray;
    byteArray.resize((int)totalBytes);
    // 复制数据
    memcpy(byteArray.data(), feature.data, totalBytes);
    return byteArray;
}

cv::Mat FeatureStorage::bytesToMat(const QByteArray &data)
{
    if (data.isEmpty()) {
        qDebug() << "数据为空，无法还原特征";
        return cv::Mat();
    }

    // 推断特征维度：常见的 SFace 模型输出 128 维 CV_32F
    // 但为了通用，根据数据大小尝试识别
    int cols = 0;
    int type = -1;

    // 检查是否为 128 维 float (512 字节)
    if (data.size() == 128 * sizeof(float)) {
        cols = 128;
        type = CV_32F;
    }
    // 检查是否为 512 维 float (2048 字节)
    else if (data.size() == 512 * sizeof(float)) {
        cols = 512;
        type = CV_32F;
    }
    // 检查是否为 128 维 double (1024 字节)
    else if (data.size() == 128 * sizeof(double)) {
        cols = 128;
        type = CV_64F;
    }
    // 其他情况尝试按 1 行处理（不确定类型）
    else {
        qDebug() << "无法识别的特征数据大小：" << data.size() << "字节";
        // 你可以根据需要补充其他尺寸
        return cv::Mat();
    }

    cv::Mat mat(1, cols, type);
    memcpy(mat.data, data.constData(), data.size());
    return mat;
}

bool FeatureStorage::isValidFeature(const QByteArray &data)
{
    if (data.isEmpty()) return false;
    // 简单判断大小是否为常见特征尺寸
    int size = data.size();
    return (size == 128 * sizeof(float)) ||
           (size == 512 * sizeof(float)) ||
           (size == 128 * sizeof(double));
}
