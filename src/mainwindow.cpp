#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "models/drivermodel.h"
#include "models/settingsmodel.h"
#include "algorithms/facerecognizer.h"
#include "algorithms/fatiguedetector.h"
#include "algorithms/livenessdetector.h"
#include "controllers/driverarchivecontroller.h"
#include "controllers/driveridentifycontroller.h"
#include "controllers/systemsettingscontroller.h"
#include "views/driverarchiveview.h"
#include "views/dmsmonitorview.h"
#include "views/systemsettingsview.h"
#include "utils/datapaths.h"

#include <QMessageBox>
#include <QDebug>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_faceRecognizer(nullptr)
    , m_fatigueDetector(nullptr)
    , m_livenessDetector(nullptr)
    , m_driverModel(nullptr)
    , m_settingsModel(nullptr)
    , m_driverArchiveController(nullptr)
    , m_driverController(nullptr)
    , m_settingsController(nullptr)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("智能座舱 · 驾驶员监测 DMS"));

    QVBoxLayout *centralLayout = new QVBoxLayout(ui->centralwidget);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->addWidget(ui->tabWidget);

    auto fillTabPage = [](QWidget *tabPage, QWidget *content) {
        if (tabPage && content && !tabPage->layout()) {
            QVBoxLayout *layout = new QVBoxLayout(tabPage);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->addWidget(content);
        }
    };
    fillTabPage(ui->tab_1, ui->pagePersonnel);
    fillTabPage(ui->tab_4, ui->pageSettings);

    initSystem();
}

MainWindow::~MainWindow()
{
    delete m_fatigueDetector;
    delete m_livenessDetector;
    delete m_faceRecognizer;
    delete m_driverModel;
    delete m_settingsModel;
    delete ui;
}

void MainWindow::initSystem()
{
    m_driverModel = new DriverModel();
    m_settingsModel = new SettingsModel();

    m_faceRecognizer = new FaceRecognizer();
    QString detectPath = findDataFile({
        "models/face_detection_yunet_2022mar.onnx",
        "face_detection_yunet_2022mar.onnx"
    });
    QString recogPath = findDataFile({
        "models/rknn/face_recognition_sface_2021dec.rknn",
        "models/face_recognition_sface_2021dec.rknn",
        "face_recognition_sface_2021dec.rknn",
        "models/face_recognition_sface_2021dec.onnx",
        "face_recognition_sface_2021dec.onnx"
    });
    if (!m_faceRecognizer->loadModels(detectPath, recogPath)) {
#ifdef Q_OS_LINUX
        qWarning() << QStringLiteral("【启动】人脸识别模型加载失败，请检查 models/ 目录");
#else
        QMessageBox::warning(this, QStringLiteral("警告"),
                             QStringLiteral("人脸识别模型加载失败，请检查路径！\n检测: %1\n识别: %2")
                                 .arg(detectPath, recogPath));
#endif
    } else {
        qInfo() << QStringLiteral("【启动】人脸识别就绪");
    }

#ifdef Q_OS_LINUX
    // S5：疲劳由 dms_ai 经 socket 推送，HMI 不再加载 pose（避免刷屏、加快启动）
    m_fatigueDetector = nullptr;
    qInfo() << QStringLiteral("【启动】疲劳监测：等待 dms_ai 推送（请先启动 dms_capture 与 dms_ai）");
#else
    m_fatigueDetector = new FatigueDetector();
    QString posePath = findDataFile({
        "models/rknn/yolov8_pose.rknn",
        "models/yolov8_pose.rknn",
        "yolov8_pose.rknn"
    });
    if (!m_fatigueDetector->init(posePath)) {
        qWarning() << QStringLiteral("【启动】疲劳模型加载失败（仍可注册/识别驾驶员）");
    }
#endif

    m_settingsController = new SystemSettingsController(this);
    m_settingsController->setModel(m_settingsModel);

    m_driverArchiveController = new DriverArchiveController(this);
    m_driverArchiveController->setModel(m_driverModel);
    m_driverArchiveController->setFaceRecognizer(m_faceRecognizer);

    m_driverController = new DriverIdentifyController(this);
    m_driverController->setDependencies(m_faceRecognizer, m_driverModel);
    m_driverController->setSettingsController(m_settingsController);

    m_livenessDetector = new LivenessDetector();
    QString lmPath = findDataFile({
        "models/face_landmarker_mask_pts5.csta",
        "3rdparty/seetaface/model/face_landmarker_mask_pts5.csta"
    });
    QString fasFirstPath = findDataFile({
        "models/fas_first.csta",
        "3rdparty/seetaface/model/fas_first.csta"
    });
    QString fasSecondPath = findDataFile({
        "models/fas_second.csta",
        "3rdparty/seetaface/model/fas_second.csta"
    });
#ifdef Q_OS_LINUX
    fasSecondPath.clear();
#endif
    if (m_livenessDetector->loadModels(lmPath, fasFirstPath, fasSecondPath)) {
        m_driverController->setLivenessDetector(m_livenessDetector);
        qInfo() << QStringLiteral("【启动】活体检测已启用");
    } else {
        qInfo() << QStringLiteral("【启动】活体检测未启用，使用纯人脸识别");
    }

    auto ensureLayout = [](QWidget *container) {
        if (container && !container->layout()) {
            auto *layout = new QVBoxLayout(container);
            layout->setContentsMargins(0, 0, 0, 0);
        }
    };
    ensureLayout(ui->pagePersonnel);
    ensureLayout(ui->pageClock);
    ensureLayout(ui->pageSettings);

    // Tab：档案 / 监控 /（运行时删掉旧「记录」页）/ 时钟 / 规则
    ui->tabWidget->setTabText(0, QStringLiteral("驾驶员档案"));
    ui->tabWidget->setTabText(1, QStringLiteral("驾驶监控"));
    ui->tabWidget->setTabText(3, QStringLiteral("系统时钟"));
    ui->tabWidget->setUsesScrollButtons(true);

    // Designer 遗留的空「考勤记录」页，运行时移除
    {
        const int idxRecord = ui->tabWidget->indexOf(ui->tab_2);
        if (idxRecord >= 0) {
            ui->tabWidget->removeTab(idxRecord);
        }
    }

    DriverArchiveView *archiveView = new DriverArchiveView(ui->pagePersonnel);
    archiveView->setController(m_driverArchiveController);
    archiveView->setFaceRecognizer(m_faceRecognizer);
    archiveView->setCameraIndex(m_settingsController->getCameraIndex());
    ui->pagePersonnel->layout()->addWidget(archiveView);

    DmsMonitorView *monitorView = new DmsMonitorView(ui->pageClock);
    monitorView->setDriverIdentifyController(m_driverController);
    monitorView->setFaceRecognizer(m_faceRecognizer);
    monitorView->setSettingsController(m_settingsController);
    monitorView->setFatigueDetector(m_fatigueDetector);
    ui->pageClock->layout()->addWidget(monitorView);

    archiveView->setBeforeCaptureCallback([monitorView]() {
        monitorView->stopCamera();
    });

    SystemSettingsView *clockSettingsView =
        new SystemSettingsView(SystemSettingsView::SectionClock, ui->pageSettings);
    clockSettingsView->setController(m_settingsController);
    ui->pageSettings->layout()->addWidget(clockSettingsView);

    QWidget *rulesTab = new QWidget(ui->tabWidget);
    auto *rulesLayout = new QVBoxLayout(rulesTab);
    rulesLayout->setContentsMargins(0, 0, 0, 0);
    SystemSettingsView *rulesView =
        new SystemSettingsView(SystemSettingsView::SectionRules, rulesTab);
    rulesView->setController(m_settingsController);
    rulesLayout->addWidget(rulesView);
    ui->tabWidget->addTab(rulesTab, QStringLiteral("识别参数"));

    const int idxMonitor = ui->tabWidget->indexOf(ui->tab_3);
    if (idxMonitor >= 0) {
        ui->tabWidget->setCurrentIndex(idxMonitor);
    }

    connect(m_driverController, &DriverIdentifyController::matchSuccess,
            this, [this](int id, const QString &name, const cv::Mat &) {
                ui->statusbar->showMessage(
                    QStringLiteral("驾驶员已确认：%1 (ID: %2)").arg(name).arg(id), 8000);
            });
}
