#ifndef FACERECOGNIZER_H
#define FACERECOGNIZER_H

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <QByteArray>
#include <QString>
#include <QDebug>
#include <memory>

class RknnEngine;

class FaceRecognizer
{
public:
    FaceRecognizer();
    ~FaceRecognizer();

    // 加载模型（检测器 + 识别器）。路径可为 .onnx 或 .rknn（识别器优先用 .rknn 走 NPU）
    bool loadModels(const QString &detectModelPath, const QString &recogModelPath);

    cv::Rect detectFace(const cv::Mat &image);

    cv::Mat extractFeature(const cv::Mat &image, bool requireBlinkCheck = true,
                           cv::Rect *outFaceRect = nullptr);

    double compareFeature(const cv::Mat &feature1, const cv::Mat &feature2);
    bool isSamePerson(const cv::Mat &feature1, const cv::Mat &feature2, double threshold = 0.363);

    static QByteArray matToByteArray(const cv::Mat &feature);
    static cv::Mat byteArrayToMat(const QByteArray &data);

    bool isReady() const;

private:
    cv::Mat alignFace112(const cv::Mat &image, const cv::Mat &faceRow) const;
    cv::Mat extractFeatureRknn(const cv::Mat &alignedBgr);
    static double cosineSimilarity(const cv::Mat &a, const cv::Mat &b);

    cv::Ptr<cv::FaceDetectorYN> detector;
    cv::Ptr<cv::FaceRecognizerSF> recognizer; // ONNX 路径；RKNN 时可为 empty
    std::unique_ptr<RknnEngine> m_rknnRecog;
    bool loaded;
    bool m_useRknnRecog;
};

#endif // FACERECOGNIZER_H
