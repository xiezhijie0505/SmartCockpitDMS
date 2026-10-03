#include "systemsettingsview.h"
#include "controllers/systemsettingscontroller.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QDateTime>

SystemSettingsView::SystemSettingsView(Section section, QWidget *parent)
    : QWidget(parent)
    , m_section(section)
    , m_cameraSpinBox(nullptr)
    , m_thresholdSpinBox(nullptr)
    , m_saveBtn(nullptr)
    , m_resetBtn(nullptr)
    , m_systemClockLabel(nullptr)
    , m_systemDateTimeEdit(nullptr)
    , m_applyTimeBtn(nullptr)
    , m_ntpSyncBtn(nullptr)
    , m_controller(nullptr)
    , m_clockTimer(nullptr)
{
    QVBoxLayout *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(10);

    switch (m_section) {
    case SectionClock:
        setupClockUi(root);
        break;
    case SectionRules:
        setupRulesUi(root);
        break;
    }
    setupSaveBar(root);
}

void SystemSettingsView::setupClockUi(QVBoxLayout *root)
{
    QGroupBox *clockGroup = new QGroupBox("系统时钟", this);
    QVBoxLayout *clockLayout = new QVBoxLayout(clockGroup);
    clockLayout->setSpacing(10);

    m_systemClockLabel = new QLabel(clockGroup);
    m_systemClockLabel->setWordWrap(true);
    clockLayout->addWidget(m_systemClockLabel);

    QFormLayout *clockForm = new QFormLayout();
    m_systemDateTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime(), clockGroup);
    m_systemDateTimeEdit->setDisplayFormat("yyyy-MM-dd HH:mm:ss");
    m_systemDateTimeEdit->setCalendarPopup(false);
    m_systemDateTimeEdit->setMinimumHeight(40);
    clockForm->addRow("校准为：", m_systemDateTimeEdit);
    clockLayout->addLayout(clockForm);

    QHBoxLayout *clockBtnLayout = new QHBoxLayout();
    m_applyTimeBtn = new QPushButton("应用系统时间", clockGroup);
    m_applyTimeBtn->setProperty("level", "success");
    m_ntpSyncBtn = new QPushButton("网络对时(NTP)", clockGroup);
    m_ntpSyncBtn->setProperty("level", "secondary");
    clockBtnLayout->addWidget(m_applyTimeBtn);
    clockBtnLayout->addWidget(m_ntpSyncBtn);
    clockLayout->addLayout(clockBtnLayout);

    QLabel *hint = new QLabel(
        "改「校准为」后点「应用系统时间」。无 RTC 电池时每次开机需校一次。",
        clockGroup);
    hint->setWordWrap(true);
    clockLayout->addWidget(hint);
    clockLayout->addStretch(1);
    root->addWidget(clockGroup, 1);

    connect(m_applyTimeBtn, &QPushButton::clicked, this, &SystemSettingsView::onApplySystemTime);
    connect(m_ntpSyncBtn, &QPushButton::clicked, this, &SystemSettingsView::onSyncNtp);

    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, &SystemSettingsView::onTickClock);
    m_clockTimer->start(1000);
    onTickClock();
}

void SystemSettingsView::setupRulesUi(QVBoxLayout *root)
{
    QGroupBox *systemGroup = new QGroupBox("识别与设备", this);
    QFormLayout *systemForm = new QFormLayout(systemGroup);
    systemForm->setSpacing(10);

    m_cameraSpinBox = new QSpinBox(systemGroup);
    m_cameraSpinBox->setRange(0, 20);
    m_cameraSpinBox->setMinimumHeight(40);
    systemForm->addRow("摄像头索引：", m_cameraSpinBox);

    m_thresholdSpinBox = new QDoubleSpinBox(systemGroup);
    m_thresholdSpinBox->setRange(0.0, 1.0);
    m_thresholdSpinBox->setSingleStep(0.01);
    m_thresholdSpinBox->setDecimals(3);
    m_thresholdSpinBox->setMinimumHeight(40);
    systemForm->addRow("人脸匹配阈值：", m_thresholdSpinBox);

    root->addWidget(systemGroup, 1);
}

void SystemSettingsView::setupSaveBar(QVBoxLayout *root)
{
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_saveBtn = new QPushButton("保存本页设置", this);
    m_saveBtn->setProperty("level", "success");
    m_resetBtn = new QPushButton("恢复全部默认", this);
    m_resetBtn->setProperty("level", "secondary");
    btnLayout->addStretch();
    btnLayout->addWidget(m_resetBtn);
    btnLayout->addWidget(m_saveBtn);
    root->addLayout(btnLayout);

    connect(m_saveBtn, &QPushButton::clicked, this, &SystemSettingsView::onSave);
    connect(m_resetBtn, &QPushButton::clicked, this, &SystemSettingsView::onReset);
}

void SystemSettingsView::setController(SystemSettingsController *controller)
{
    m_controller = controller;
    if (m_controller) {
        loadSettings();
        connect(m_controller, &SystemSettingsController::settingsReset,
                this, &SystemSettingsView::loadSettings);
    }
}

void SystemSettingsView::loadSettings()
{
    if (!m_controller) {
        return;
    }

    if (m_section == SectionClock && m_systemDateTimeEdit) {
        m_systemDateTimeEdit->setDateTime(QDateTime::currentDateTime());
    }

    if (m_section == SectionRules) {
        if (m_cameraSpinBox) {
            m_cameraSpinBox->setValue(m_controller->getCameraIndex());
        }
        if (m_thresholdSpinBox) {
            m_thresholdSpinBox->setValue(m_controller->getFaceMatchThreshold());
        }
    }
}

void SystemSettingsView::onSave()
{
    if (!m_controller) {
        return;
    }

    if (m_section == SectionRules) {
        if (m_cameraSpinBox) {
            m_controller->setCameraIndex(m_cameraSpinBox->value());
        }
        if (m_thresholdSpinBox) {
            m_controller->setFaceMatchThreshold(m_thresholdSpinBox->value());
        }
        showMessage("识别与设备设置已保存");
    } else {
        showMessage("本页无需保存，请用「应用系统时间」或「网络对时」");
    }
}

void SystemSettingsView::onReset()
{
    if (!m_controller) {
        return;
    }
    m_controller->resetToDefaults();
    loadSettings();
    showMessage("已恢复默认配置");
}

void SystemSettingsView::onApplySystemTime()
{
    if (!m_controller || !m_systemDateTimeEdit) {
        return;
    }
    QString err;
    if (m_controller->setSystemDateTime(m_systemDateTimeEdit->dateTime(), &err)) {
        showMessage("系统时间已更新");
        onTickClock();
    } else {
        showMessage(err.isEmpty() ? "设置系统时间失败" : err, false);
    }
}

void SystemSettingsView::onSyncNtp()
{
    if (!m_controller) {
        return;
    }
    QString err;
    if (m_controller->syncSystemTimeFromNtp(&err)) {
        if (m_systemDateTimeEdit) {
            m_systemDateTimeEdit->setDateTime(QDateTime::currentDateTime());
        }
        showMessage("NTP 对时成功");
        onTickClock();
    } else {
        showMessage(err.isEmpty() ? "NTP 对时失败" : err, false);
    }
}

void SystemSettingsView::onTickClock()
{
    if (!m_systemClockLabel) {
        return;
    }
    m_systemClockLabel->setText(
        QStringLiteral("当前系统时间：%1")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")));
}

void SystemSettingsView::showMessage(const QString &msg, bool success)
{
    QMessageBox box(this);
    box.setIcon(success ? QMessageBox::Information : QMessageBox::Warning);
    box.setWindowTitle(success ? "成功" : "失败");
    box.setText(msg);
    box.exec();
}
