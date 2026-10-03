#ifndef FEATURESTORAGE_H
#define FEATURESTORAGE_H

#include <QByteArray>
#include <opencv2/opencv.hpp>

class FeatureStorage
{
public:
    // 将 cv::Mat 特征转为 QByteArray（用于存入数据库）
    static QByteArray matToBytes(const cv::Mat &feature);

    // 从 QByteArray 还原为 cv::Mat
    // 需要指定预期的维度（如果数据格式固定，可以自动推断）
    static cv::Mat bytesToMat(const QByteArray &data);

    // 检查 QByteArray 是否包含有效的特征数据
    static bool isValidFeature(const QByteArray &data);
};

#endif // FEATURESTORAGE_H
