#include "driveradddialog.h"
#include "facecapturedialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QFrame>
#include <QGuiApplication>
#include <QScreen>
#include <QDebug>
#include "algorithms/facerecognizer.h"
#include <opencv2/opencv.hpp>

DriverAddDialog::DriverAddDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUI();
    m_statusCombo->setCurrentIndex(0);
}

void DriverAddDialog::setupUI()
{
    setModal(true);
    setWindowTitle(tr("驾驶员信息"));

    // 按屏幕可用区域自适应，避免嵌入式小屏裁掉底部「确定」
    QSize target(480, 420);
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        target.setWidth(qMin(500, qMax(280, avail.width() - 24)));
        target.setHeight(qMin(480, qMax(320, avail.height() - 24)));
    }
    resize(target);
    setMinimumSize(260, 280);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // 可滚动内容区：表单再高也不会挡住底部按钮
    QWidget *scrollContent = new QWidget(this);
    QVBoxLayout *contentLayout = new QVBoxLayout(scrollContent);
    contentLayout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *infoGroup = new QGroupBox("基本信息", scrollContent);
    QFormLayout *formLayout = new QFormLayout(infoGroup);
    formLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    m_nameEdit = new QLineEdit(scrollContent);
    formLayout->addRow("姓名 (*)：", m_nameEdit);

    m_genderCombo = new QComboBox(scrollContent);
    m_genderCombo->addItems({"男", "女"});
    formLayout->addRow("性别：", m_genderCombo);

    m_departmentEdit = new QLineEdit(scrollContent);
    formLayout->addRow("部门：", m_departmentEdit);

    m_positionEdit = new QLineEdit(scrollContent);
    formLayout->addRow("职位：", m_positionEdit);

    m_phoneEdit = new QLineEdit(scrollContent);
    formLayout->addRow("手机号：", m_phoneEdit);

    m_statusCombo = new QComboBox(scrollContent);
    m_statusCombo->addItems({"在职", "离职", "试用期"});
    formLayout->addRow("状态：", m_statusCombo);

    m_remarksEdit = new QLineEdit(scrollContent);
    formLayout->addRow("备注：", m_remarksEdit);

    contentLayout->addWidget(infoGroup);

    QGroupBox *faceGroup = new QGroupBox("人脸特征", scrollContent);
    QVBoxLayout *faceLayout = new QVBoxLayout(faceGroup);

    QHBoxLayout *faceBtnLayout = new QHBoxLayout();
    m_captureFaceBtn = new QPushButton("从摄像头采集", scrollContent);
    m_loadFaceBtn = new QPushButton("从图片加载", scrollContent);
    m_loadFaceBtn->setProperty("level", "secondary");
    m_clearFaceBtn = new QPushButton("清除", scrollContent);
    m_clearFaceBtn->setProperty("level", "secondary");
    faceBtnLayout->addWidget(m_captureFaceBtn);
    faceBtnLayout->addWidget(m_loadFaceBtn);
    faceBtnLayout->addWidget(m_clearFaceBtn);
    faceLayout->addLayout(faceBtnLayout);

    m_facePreviewLabel = new QLabel(scrollContent);
    m_facePreviewLabel->setObjectName("facePreview");
    m_facePreviewLabel->setMinimumHeight(80);
    m_facePreviewLabel->setMaximumHeight(120);
    m_facePreviewLabel->setAlignment(Qt::AlignCenter);
    m_facePreviewLabel->setText("未采集人脸");
    faceLayout->addWidget(m_facePreviewLabel);

    contentLayout->addWidget(faceGroup);
    contentLayout->addStretch();

    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("personnelScroll");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(scrollContent);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    // 竖条始终显示，避免「要滑才出现」；样式里已加宽到约 22px
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    if (QScrollBar *vbar = scrollArea->verticalScrollBar()) {
        vbar->setSingleStep(20);
        vbar->setPageStep(120);
    }
    mainLayout->addWidget(scrollArea, 1);

    // 确定/取消固定在对话框底部，始终可见
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_okBtn = new QPushButton("确定", this);
    m_cancelBtn = new QPushButton("取消", this);
    m_cancelBtn->setProperty("level", "secondary");
    m_okBtn->setMinimumHeight(36);
    m_cancelBtn->setMinimumHeight(36);
    btnLayout->addWidget(m_okBtn, 1);
    btnLayout->addWidget(m_cancelBtn, 1);
    mainLayout->addLayout(btnLayout);

    connect(m_okBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_captureFaceBtn, &QPushButton::clicked, this, &DriverAddDialog::onCaptureFace);
    connect(m_loadFaceBtn, &QPushButton::clicked, this, &DriverAddDialog::onLoadFaceImage);
    connect(m_clearFaceBtn, &QPushButton::clicked, this, &DriverAddDialog::onClearFace);
}

QVariantMap DriverAddDialog::getPersonData() const
{
    QVariantMap data;
    data["name"] = m_nameEdit->text().trimmed();
    data["gender"] = m_genderCombo->currentText();
    data["department"] = m_departmentEdit->text().trimmed();
    data["position"] = m_positionEdit->text().trimmed();
    data["phone"] = m_phoneEdit->text().trimmed();
    data["status"] = m_statusCombo->currentText();
    data["remarks"] = m_remarksEdit->text().trimmed();
    return data;
}

QByteArray DriverAddDialog::getFaceFeature() const
{
    return m_faceFeature;
}

void DriverAddDialog::setPersonData(const QVariantMap &data)
{
    m_nameEdit->setText(data["name"].toString());
    m_genderCombo->setCurrentText(data["gender"].toString());
    m_departmentEdit->setText(data["department"].toString());
    m_positionEdit->setText(data["position"].toString());
    m_phoneEdit->setText(data["phone"].toString());
    m_statusCombo->setCurrentText(data["status"].toString());
    m_remarksEdit->setText(data["remarks"].toString());
}

void DriverAddDialog::onCaptureFace()
{
    if (!m_faceRecognizer || !m_faceRecognizer->isReady()) {
        QMessageBox::warning(this, "错误", "人脸识别模块未初始化，无法提取特征");
        return;
    }

    if (m_beforeCapture) {
        m_beforeCapture();
    }

    FaceCaptureDialog captureDlg(m_faceRecognizer, m_cameraIndex, this);
    if (captureDlg.exec() != QDialog::Accepted) {
        return;
    }

    m_faceFeature = captureDlg.faceFeature();
    if (m_faceFeature.isEmpty()) {
        QMessageBox::warning(this, "提示", "采集失败，未得到有效人脸特征");
        return;
    }

    applyFacePreview(captureDlg.previewPixmap());
    QMessageBox::information(this, "成功", "人脸特征已从摄像头提取！");
}

void DriverAddDialog::onLoadFaceImage()
{
    QString path = QFileDialog::getOpenFileName(this, "选择包含人脸的图片", "",
                                                "Images (*.jpg *.jpeg *.png *.bmp)");
    if (path.isEmpty()) return;

    cv::Mat img = cv::imread(path.toStdString());
    if (img.empty()) {
        QMessageBox::warning(this, "错误", "无法读取图片文件");
        return;
    }

    if (!m_faceRecognizer || !m_faceRecognizer->isReady()) {
        QMessageBox::warning(this, "错误", "人脸识别模块未初始化，无法提取特征");
        return;
    }

    cv::Mat feature = m_faceRecognizer->extractFeature(img, false);
    if (feature.empty()) {
        QMessageBox::warning(this, "提示", "图片中未检测到人脸，请换一张清晰的正脸照片");
        return;
    }

    m_faceFeature = FaceRecognizer::matToByteArray(feature);

    QPixmap pixmap(path);
    applyFacePreview(pixmap);

    QMessageBox::information(this, "成功", "人脸特征已提取并保存！");
}

void DriverAddDialog::onClearFace()
{
    m_faceFeature.clear();
    m_facePreviewLabel->clear();
    m_facePreviewLabel->setText("未采集人脸");
}

void DriverAddDialog::setFaceRecognizer(FaceRecognizer *recognizer)
{
    m_faceRecognizer = recognizer;
}

void DriverAddDialog::setCameraIndex(int index)
{
    m_cameraIndex = index;
}

void DriverAddDialog::setBeforeCaptureCallback(const std::function<void()> &cb)
{
    m_beforeCapture = cb;
}

void DriverAddDialog::applyFacePreview(const QPixmap &pixmap)
{
    if (pixmap.isNull()) {
        m_facePreviewLabel->setText("已采集人脸");
        return;
    }
    m_facePreviewLabel->setPixmap(pixmap.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_facePreviewLabel->setText("");
}
