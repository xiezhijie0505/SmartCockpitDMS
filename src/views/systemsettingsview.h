#ifndef SYSTEMSETTINGSVIEW_H
#define SYSTEMSETTINGSVIEW_H

#include <QWidget>
#include <QDateTimeEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QTimer>

class QVBoxLayout;
class SystemSettingsController;

class SystemSettingsView : public QWidget
{
    Q_OBJECT

public:
    enum Section {
        SectionClock = 0,
        SectionRules
    };

    explicit SystemSettingsView(Section section, QWidget *parent = nullptr);
    void setController(SystemSettingsController *controller);

private slots:
    void onSave();
    void onReset();
    void onApplySystemTime();
    void onSyncNtp();
    void onTickClock();

private:
    void setupClockUi(QVBoxLayout *root);
    void setupRulesUi(QVBoxLayout *root);
    void setupSaveBar(QVBoxLayout *root);
    void loadSettings();
    void showMessage(const QString &msg, bool success = true);

    Section m_section;

    QSpinBox *m_cameraSpinBox;
    QDoubleSpinBox *m_thresholdSpinBox;
    QPushButton *m_saveBtn;
    QPushButton *m_resetBtn;

    QLabel *m_systemClockLabel;
    QDateTimeEdit *m_systemDateTimeEdit;
    QPushButton *m_applyTimeBtn;
    QPushButton *m_ntpSyncBtn;

    SystemSettingsController *m_controller;
    QTimer *m_clockTimer;
};

#endif // SYSTEMSETTINGSVIEW_H
