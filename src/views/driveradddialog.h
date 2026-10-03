#ifndef DRIVERADDDIALOG_H
#define DRIVERADDDIALOG_H

#include <QDialog>
#include <QVariantMap>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QByteArray>
#include <QPixmap>
#include <functional>
#include "algorithms/facerecognizer.h"


class DriverAddDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DriverAddDialog(QWidget *parent = nullptr);

    QVariantMap getPersonData() const;
    void setPersonData(const QVariantMap &data);
    void setFaceRecognizer(FaceRecognizer *recognizer);
    void setCameraIndex(int index);
    void setBeforeCaptureCallback(const std::function<void()> &cb);
    QByteArray getFaceFeature() const;

private slots:
    void onCaptureFace();
    void onLoadFaceImage();
    void onClearFace();

private:
    void setupUI();
    void applyFacePreview(const QPixmap &pixmap);

    QLineEdit *m_nameEdit;
    QComboBox *m_genderCombo;
    QLineEdit *m_departmentEdit;
    QLineEdit *m_positionEdit;
    QLineEdit *m_phoneEdit;
    QComboBox *m_statusCombo;
    QLineEdit *m_remarksEdit;

    QPushButton *m_captureFaceBtn;
    QPushButton *m_loadFaceBtn;
    QPushButton *m_clearFaceBtn;
    QLabel *m_facePreviewLabel;

    QPushButton *m_okBtn;
    QPushButton *m_cancelBtn;

    QByteArray m_faceFeature;
    FaceRecognizer *m_faceRecognizer = nullptr;
    int m_cameraIndex = 0;
    std::function<void()> m_beforeCapture;
};

#endif // DRIVERADDDIALOG_H
