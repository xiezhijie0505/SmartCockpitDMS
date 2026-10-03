#include "facerecognizer.h"
#include "rknnengine.h"
#include <QFileInfo>
#include <cstring>
#include <cmath>

FaceRecognizer::FaceRecognizer()
    : loaded(false)
    , m_useRknnRecog(false)
{
}

FaceRecognizer::~FaceRecognizer() = default;

bool FaceRecognizer::isReady() const
{
    if (detector.empty()) {
        return false;
    }
    if (m_useRknnRecog) {
        return m_rknnRecog && m_rknnRecog->isReady();
    }
    return !recognizer.empty();
}

bool FaceRecognizer::loadModels(const QString &detectModelPath, const QString &recogModelPath)
{
    loaded = false;
    m_useRknnRecog = false;
    m_rknnRecog.reset();
    detector.release();
    recognizer.release();

    if (!QFileInfo::exists(detectModelPath)) {
        qWarning() << "检测模型不存在：" << detectModelPath;
        return false;
    }
    if (!QFileInfo::exists(recogModelPath)) {
        qWarning() << "识别模型不存在：" << recogModelPath;
        return false;
    }

    if (detectModelPath.endsWith(QLatin1String(".rknn"), Qt::CaseInsensitive)) {
        qWarning() << "检测暂不支持 .rknn，请使用 YuNet .onnx:" << detectModelPath;
        return false;
    }

    // 嵌入式摄像头画面偏暗时 0.9 过严，略降以免“检不到人”
    detector = cv::FaceDetectorYN::create(
        detectModelPath.toStdString(),
        "",
        cv::Size(320, 240),
        0.7f,
        0.3f,
        5000);

    if (detector.empty()) {
        qWarning() << "人脸检测模型加载失败";
        return false;
    }

    auto loadOnnxRecog = [this](const QString &path) -> bool {
        recognizer = cv::FaceRecognizerSF::create(path.toStdString(), "");
        if (recognizer.empty()) {
            qWarning() << "OpenCV SFace 加载失败" << path;
            return false;
        }
        m_useRknnRecog = false;
        loaded = true;
        qInfo() << QStringLiteral("【启动】人脸识别：YuNet + OpenCV SFace");
        return true;
    };

    if (recogModelPath.endsWith(QLatin1String(".rknn"), Qt::CaseInsensitive)) {
        m_rknnRecog.reset(new RknnEngine());
        if (m_rknnRecog->load(recogModelPath)) {
            m_useRknnRecog = true;
            loaded = true;
            qInfo() << QStringLiteral("【启动】人脸识别：YuNet + RKNN SFace");
            return true;
        }
        qWarning() << "RKNN 加载失败，尝试回退 ONNX。请确认 LD_LIBRARY_PATH 指向 2.3.2 的 librknnrt.so";
        m_rknnRecog.reset();
        m_useRknnRecog = false;

        // models/rknn/xxx.rknn -> models/xxx.onnx 或同目录换后缀
        QStringList fallbacks;
        QString base = QFileInfo(recogModelPath).fileName();
        base.replace(QLatin1String(".rknn"), QLatin1String(".onnx"), Qt::CaseInsensitive);
        fallbacks << QFileInfo(recogModelPath).absolutePath() + QLatin1String("/../") + base;
        fallbacks << QFileInfo(recogModelPath).absolutePath() + QLatin1Char('/') + base;
        fallbacks << QLatin1String("models/") + base;
        for (QString fb : fallbacks) {
            fb = QFileInfo(fb).absoluteFilePath();
            if (QFileInfo::exists(fb) && loadOnnxRecog(fb)) {
                return true;
            }
        }
        qWarning() << "RKNN 与 ONNX 识别模型均不可用";
        return false;
    }

    return loadOnnxRecog(recogModelPath);
}

cv::Rect FaceRecognizer::detectFace(const cv::Mat &image)
{
    if (!isReady() || image.empty()) {
        return cv::Rect();
    }

    detector->setInputSize(cv::Size(image.cols, image.rows));
    cv::Mat faces;
    detector->detect(image, faces);
    if (faces.rows == 0) {
        return cv::Rect();
    }

    float x = faces.at<float>(0, 0);
    float y = faces.at<float>(0, 1);
    float w = faces.at<float>(0, 2);
    float h = faces.at<float>(0, 3);
    return cv::Rect(static_cast<int>(x), static_cast<int>(y),
                    static_cast<int>(w), static_cast<int>(h));
}

cv::Mat FaceRecognizer::alignFace112(const cv::Mat &image, const cv::Mat &faceRow) const
{
    // YuNet 行：x,y,w,h,score + 5 个关键点 (右眼,左眼,鼻,右嘴角,左嘴角)
    if (faceRow.cols < 15) {
        return cv::Mat();
    }
    std::vector<cv::Point2f> src(5);
    for (int i = 0; i < 5; ++i) {
        src[i] = cv::Point2f(faceRow.at<float>(0, 5 + i * 2),
                             faceRow.at<float>(0, 5 + i * 2 + 1));
    }
    // ArcFace/SFace 112x112 标准对齐点
    std::vector<cv::Point2f> dst = {
        {38.2946f, 51.6963f},
        {73.5318f, 51.5014f},
        {56.0252f, 71.7366f},
        {41.5493f, 92.3655f},
        {70.7299f, 92.2041f},
    };
    cv::Mat m = cv::estimateAffinePartial2D(src, dst, cv::noArray(), cv::LMEDS);
    if (m.empty()) {
        return cv::Mat();
    }
    cv::Mat aligned;
    cv::warpAffine(image, aligned, m, cv::Size(112, 112),
                   cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    return aligned;
}

cv::Mat FaceRecognizer::extractFeatureRknn(const cv::Mat &alignedBgr)
{
    QVector<cv::Mat> outs;
    if (!m_rknnRecog || !m_rknnRecog->infer(alignedBgr, &outs) || outs.isEmpty()) {
        return cv::Mat();
    }
    cv::Mat feature = outs[0].reshape(1, 1).clone();
    // L2 normalize，便于余弦比对
    cv::Mat feature64;
    feature.convertTo(feature64, CV_32F);
    const double n = cv::norm(feature64);
    if (n > 1e-6) {
        feature64 /= n;
    }
    return feature64;
}

double FaceRecognizer::cosineSimilarity(const cv::Mat &a, const cv::Mat &b)
{
    if (a.empty() || b.empty() || a.total() != b.total()) {
        return -1.0;
    }
    cv::Mat af, bf;
    a.convertTo(af, CV_32F);
    b.convertTo(bf, CV_32F);
    af = af.reshape(1, 1);
    bf = bf.reshape(1, 1);
    double na = cv::norm(af);
    double nb = cv::norm(bf);
    if (na < 1e-6 || nb < 1e-6) {
        return -1.0;
    }
    return af.dot(bf) / (na * nb);
}

cv::Mat FaceRecognizer::extractFeature(const cv::Mat &image, bool requireBlinkCheck, cv::Rect *outFaceRect)
{
    Q_UNUSED(requireBlinkCheck);
    if (!isReady() || image.empty()) {
        return cv::Mat();
    }

    detector->setInputSize(cv::Size(image.cols, image.rows));
    cv::Mat faces;
    detector->detect(image, faces);
    if (faces.rows == 0) {
        return cv::Mat();
    }

    if (outFaceRect) {
        *outFaceRect = cv::Rect(static_cast<int>(faces.at<float>(0, 0)),
                                static_cast<int>(faces.at<float>(0, 1)),
                                static_cast<int>(faces.at<float>(0, 2)),
                                static_cast<int>(faces.at<float>(0, 3)));
    }

    if (m_useRknnRecog) {
        cv::Mat aligned = alignFace112(image, faces.row(0));
        if (aligned.empty()) {
            qWarning() << "人脸对齐失败（关键点不足或估计失败）";
            return cv::Mat();
        }
        cv::Mat feat = extractFeatureRknn(aligned);
        if (feat.empty()) {
            qWarning() << QStringLiteral("【识别】提特征失败");
        }
        return feat;
    }

    cv::Mat alignedFace;
    recognizer->alignCrop(image, faces.row(0), alignedFace);
    if (alignedFace.empty()) {
        return cv::Mat();
    }
    cv::Mat feature;
    recognizer->feature(alignedFace, feature);
    if (feature.empty()) {
        return cv::Mat();
    }
    return feature.clone();
}

double FaceRecognizer::compareFeature(const cv::Mat &feature1, const cv::Mat &feature2)
{
    if (feature1.empty() || feature2.empty()) {
        return -1.0;
    }
    if (m_useRknnRecog || recognizer.empty()) {
        return cosineSimilarity(feature1, feature2);
    }
    return recognizer->match(feature1, feature2, cv::FaceRecognizerSF::FR_COSINE);
}

bool FaceRecognizer::isSamePerson(const cv::Mat &feature1, const cv::Mat &feature2, double threshold)
{
    double score = compareFeature(feature1, feature2);
    if (score < 0) {
        return false;
    }
    return (score >= threshold);
}

QByteArray FaceRecognizer::matToByteArray(const cv::Mat &feature)
{
    if (feature.empty()) {
        return QByteArray();
    }
    size_t totalBytes = feature.total() * feature.elemSize();
    QByteArray byteArray;
    byteArray.resize(static_cast<int>(totalBytes));
    memcpy(byteArray.data(), feature.data, totalBytes);
    return byteArray;
}

cv::Mat FaceRecognizer::byteArrayToMat(const QByteArray &data)
{
    if (data.isEmpty()) {
        return cv::Mat();
    }

    int rows = 1;
    int cols = 128;
    int type = CV_32F;

    if (data.size() == 128 * static_cast<int>(sizeof(float))) {
        cols = 128;
        type = CV_32F;
    } else if (data.size() == 512 * static_cast<int>(sizeof(float))) {
        cols = 512;
        type = CV_32F;
    } else if (data.size() == 128 * static_cast<int>(sizeof(double))) {
        cols = 128;
        type = CV_64F;
    } else {
        qDebug() << "无法识别特征数据大小：" << data.size();
        return cv::Mat();
    }

    cv::Mat mat(rows, cols, type);
    memcpy(mat.data, data.data(), static_cast<size_t>(data.size()));
    return mat;
}
