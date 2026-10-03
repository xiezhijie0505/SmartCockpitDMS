#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class FaceRecognizer;
class FatigueDetector;
class LivenessDetector;
class DriverModel;
class SettingsModel;
class DriverArchiveController;
class DriverIdentifyController;
class SystemSettingsController;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    void initSystem();

    Ui::MainWindow *ui;
    FaceRecognizer *m_faceRecognizer;
    FatigueDetector *m_fatigueDetector;
    LivenessDetector *m_livenessDetector;
    DriverModel *m_driverModel;
    SettingsModel *m_settingsModel;
    DriverArchiveController *m_driverArchiveController;
    DriverIdentifyController *m_driverController;
    SystemSettingsController *m_settingsController;
};

#endif // MAINWINDOW_H
