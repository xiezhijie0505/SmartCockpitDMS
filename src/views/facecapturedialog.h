#ifndef FACECAPTUREDIALOG_H
#define FACECAPTUREDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QByteArray>
#include <QPixmap>
#include <opencv2/opencv.hpp>

class FaceRecognizer;
class FrameShmReader;

class FaceCaptureDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FaceCaptureDialog(FaceRecognizer *recognizer, int cameraIndex, QWidget *parent = nullptr);
    ~FaceCaptureDialog() override;

    QByteArray faceFeature() const { return m_faceFeature; }
    QPixmap previewPixmap() const { return m_previewPixmap; }

private slots:
    void onUpdateFrame();
    void onCapture();

private:
    bool openCamera();
    void closeCamera();

    FaceRecognizer *m_recognizer = nullptr;
    int m_cameraIndex = 0;

    QLabel *m_videoLabel = nullptr;
    QPushButton *m_captureBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
    QTimer *m_timer = nullptr;

    FrameShmReader *m_shmReader = nullptr;
    bool m_useShm = false;
    cv::VideoCapture m_cap;
    cv::Mat m_currentFrame;
    QByteArray m_faceFeature;
    QPixmap m_previewPixmap;
};

#endif // FACECAPTUREDIALOG_H
