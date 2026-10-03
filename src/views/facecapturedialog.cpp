#include "facecapturedialog.h"
#include "algorithms/facerecognizer.h"
#include "ipc/frame_shm_reader.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QGuiApplication>
#include <QScreen>
#include <QImage>

FaceCaptureDialog::FaceCaptureDialog(FaceRecognizer *recognizer, int cameraIndex, QWidget *parent)
    : QDialog(parent)
    , m_recognizer(recognizer)
    , m_cameraIndex(cameraIndex)
    , m_shmReader(new FrameShmReader())
{
    setModal(true);
    setWindowTitle(tr("摄像头采集人脸"));

    QSize target(420, 360);
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        target.setWidth(qMin(480, qMax(280, avail.width() - 24)));
        target.setHeight(qMin(420, qMax(300, avail.height() - 24)));
    }
    resize(target);
    setMinimumSize(260, 240);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    m_videoLabel = new QLabel(this);
    m_videoLabel->setObjectName("faceCapturePreview");
    m_videoLabel->setAlignment(Qt::AlignCenter);
    m_videoLabel->setMinimumHeight(180);
    m_videoLabel->setText(tr("正在打开摄像头…"));
    m_videoLabel->setStyleSheet("background:#1a1a1a;color:#ccc;");
    layout->addWidget(m_videoLabel, 1);

    QLabel *hint = new QLabel(tr("请正对摄像头，画面稳定后点击「采集」"), this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_captureBtn = new QPushButton(tr("采集"), this);
    m_cancelBtn = new QPushButton(tr("取消"), this);
    m_cancelBtn->setProperty("level", "secondary");
    m_captureBtn->setMinimumHeight(36);
    m_cancelBtn->setMinimumHeight(36);
    m_captureBtn->setEnabled(false);
    btnLayout->addWidget(m_captureBtn, 1);
    btnLayout->addWidget(m_cancelBtn, 1);
    layout->addLayout(btnLayout);

    connect(m_captureBtn, &QPushButton::clicked, this, &FaceCaptureDialog::onCapture);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &FaceCaptureDialog::onUpdateFrame);

    if (!openCamera()) {
        QMessageBox::warning(this, tr("错误"),
                             tr("无法打开画面。\n"
                                "Linux 请先启动 dms_capture（读共享内存）；\n"
                                "或确认本机摄像头未被占用。"));
        QTimer::singleShot(0, this, &QDialog::reject);
        return;
    }

    m_captureBtn->setEnabled(true);
    m_timer->start(33);
}

FaceCaptureDialog::~FaceCaptureDialog()
{
    closeCamera();
    delete m_shmReader;
    m_shmReader = nullptr;
}

bool FaceCaptureDialog::openCamera()
{
#ifdef Q_OS_LINUX
    // S4.2：与监控页一致，读 dms_capture 的 shm，不抢 V4L2
    if (m_shmReader && m_shmReader->open()) {
        m_useShm = true;
        return true;
    }
    m_useShm = false;
#endif

    if (m_cap.isOpened()) {
        m_cap.release();
    }

    m_cap.open(m_cameraIndex);
    if (!m_cap.isOpened()) {
        const int fallbacks[] = {9, 10, 1, 2, 11, 0};
        for (int idx : fallbacks) {
            if (idx == m_cameraIndex) {
                continue;
            }
            m_cap.open(idx);
            if (m_cap.isOpened()) {
                m_cameraIndex = idx;
                break;
            }
        }
    }
    if (!m_cap.isOpened()) {
        return false;
    }

    m_cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'));
    m_cap.set(cv::CAP_PROP_FRAME_WIDTH, 320);
    m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, 240);
    m_cap.set(cv::CAP_PROP_FPS, 15);
    m_cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
    return true;
}

void FaceCaptureDialog::closeCamera()
{
    if (m_timer) {
        m_timer->stop();
    }
#ifdef Q_OS_LINUX
    if (m_shmReader) {
        m_shmReader->close();
    }
    m_useShm = false;
#endif
    if (m_cap.isOpened()) {
        m_cap.release();
    }
}

void FaceCaptureDialog::onUpdateFrame()
{
    cv::Mat frame;
#ifdef Q_OS_LINUX
    if (m_useShm) {
        if (!m_shmReader || !m_shmReader->copyLatest(frame) || frame.empty()) {
            return;
        }
    } else
#endif
    {
        if (!m_cap.isOpened()) {
            return;
        }
        m_cap.grab();
        if (!m_cap.retrieve(frame) || frame.empty()) {
            if (!m_cap.read(frame) || frame.empty()) {
                return;
            }
        }
    }

    m_currentFrame = frame;

    cv::Mat show;
    if (frame.cols > 320 || frame.rows > 240) {
        cv::resize(frame, show, cv::Size(320, 240), 0, 0, cv::INTER_NEAREST);
    } else {
        show = frame;
    }

    cv::Mat rgb;
    cv::cvtColor(show, rgb, cv::COLOR_BGR2RGB);
    QImage qimg(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
    m_videoLabel->setPixmap(QPixmap::fromImage(qimg.copy()).scaled(
        m_videoLabel->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
}

void FaceCaptureDialog::onCapture()
{
    if (m_currentFrame.empty()) {
        QMessageBox::warning(this, tr("提示"), tr("尚未获取到画面，请稍候再试"));
        return;
    }
    if (!m_recognizer || !m_recognizer->isReady()) {
        QMessageBox::warning(this, tr("错误"), tr("人脸识别模块未初始化，无法提取特征"));
        return;
    }

    cv::Rect faceRect;
    cv::Mat feature = m_recognizer->extractFeature(m_currentFrame, false, &faceRect);
    if (feature.empty()) {
        QMessageBox::warning(this, tr("提示"), tr("未检测到人脸，请正对摄像头后重试"));
        return;
    }

    m_faceFeature = FaceRecognizer::matToByteArray(feature);

    cv::Mat preview = m_currentFrame.clone();
    if (faceRect.width > 0 && faceRect.height > 0) {
        cv::rectangle(preview, faceRect, cv::Scalar(0, 255, 0), 2);
    }
    cv::Mat rgb;
    cv::cvtColor(preview, rgb, cv::COLOR_BGR2RGB);
    QImage qimg(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
    m_previewPixmap = QPixmap::fromImage(qimg.copy());

    closeCamera();
    accept();
}
