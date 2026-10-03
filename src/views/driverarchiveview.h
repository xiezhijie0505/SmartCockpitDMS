#ifndef DRIVERARCHIVEVIEW_H
#define DRIVERARCHIVEVIEW_H

#include <QWidget>
#include <QTableView>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStandardItemModel>
#include "algorithms/facerecognizer.h"
#include <QFileDialog>
#include <QDir>
#include <QProgressDialog>
#include <QMessageBox>
#include <functional>
// 前置声明
class DriverArchiveController;

class DriverArchiveView : public QWidget
{
    Q_OBJECT

public:
    explicit DriverArchiveView(QWidget *parent = nullptr);
    ~DriverArchiveView();

    // 设置控制器（由 MainWindow 注入）
    void setController(DriverArchiveController *controller);
    void setFaceRecognizer(FaceRecognizer *recognizer);
    void setCameraIndex(int index);
    void setBeforeCaptureCallback(const std::function<void()> &cb);

private slots:
    // 按钮槽函数
    void onAddPerson();
    void onEditPerson();
    void onDeletePerson();
    void onSearchPersons();
    void onRefreshTable();

    // 表格选择变化
    void onTableSelectionChanged();
    //添加“批量导入”按钮
    void onBatchImport();

private:
    // 初始化界面
    void setupUI();

    // 加载数据到表格
    void loadTableData(const QList<QVariantMap> &data);

    // 显示提示信息
    void showMessage(const QString &msg, bool isError = false);

    // UI 控件
    QTableView *m_tableView;
    QStandardItemModel *m_tableModel;

    QLineEdit *m_searchEdit;
    QPushButton *m_searchBtn;
    QPushButton *m_addBtn;
    QPushButton *m_editBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_refreshBtn;

    // 控制器
    DriverArchiveController *m_controller;
    FaceRecognizer *m_faceRecognizer = nullptr;
    QPushButton *m_batchImportBtn;
    int m_cameraIndex = 0;
    std::function<void()> m_beforeCapture;
};

#endif // DRIVERARCHIVEVIEW_H
