#include "driverarchiveview.h"
#include "controllers/driverarchivecontroller.h"
#include "views/driveradddialog.h"  // 添加/编辑驾驶员对话框（后续实现）
#include <QMessageBox>
#include <QHeaderView>
#include <QDebug>

DriverArchiveView::DriverArchiveView(QWidget *parent)
    : QWidget(parent)
    , m_controller(nullptr)
{
    setupUI();
    loadTableData(QList<QVariantMap>());
}

DriverArchiveView::~DriverArchiveView()
{
}

void DriverArchiveView::setupUI()
{
    // ========== 主布局（垂直） ==========
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // ========== 顶部：搜索栏 ==========
    QHBoxLayout *searchLayout = new QHBoxLayout();
    QLabel *searchLabel = new QLabel("搜索：", this);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("输入姓名搜索...");
    m_searchBtn = new QPushButton("搜索", this);
    m_refreshBtn = new QPushButton("刷新", this);
    m_refreshBtn->setProperty("level", "secondary");

    searchLayout->addWidget(searchLabel);
    searchLayout->addWidget(m_searchEdit);
    searchLayout->addWidget(m_searchBtn);
    searchLayout->addStretch();
    searchLayout->addWidget(m_refreshBtn);

    mainLayout->addLayout(searchLayout);

    // ========== 中间：表格 ==========
    m_tableView = new QTableView(this);
    m_tableModel = new QStandardItemModel(this);
    m_tableModel->setHorizontalHeaderLabels(QStringList()
        << "ID" << "姓名" << "性别" << "部门" << "职位"
        << "手机号" << "状态" << "备注");

    m_tableView->setModel(m_tableModel);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    mainLayout->addWidget(m_tableView);

    // ========== 底部：操作按钮 ==========
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    // 底部按钮布局中添加批量导入按钮
        m_batchImportBtn = new QPushButton("批量导入", this);
        m_batchImportBtn->setProperty("level", "success");

    m_addBtn = new QPushButton(QStringLiteral("注册驾驶员"), this);
    m_editBtn = new QPushButton(QStringLiteral("编辑驾驶员"), this);
    m_editBtn->setProperty("level", "secondary");
    m_deleteBtn = new QPushButton(QStringLiteral("删除驾驶员"), this);
    m_deleteBtn->setProperty("level", "danger");
    m_editBtn->setEnabled(false);
    m_deleteBtn->setEnabled(false);

    buttonLayout->addWidget(m_addBtn);
    buttonLayout->addWidget(m_editBtn);
    buttonLayout->addWidget(m_deleteBtn);

    buttonLayout->addWidget(m_batchImportBtn);

    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);

    // ========== 连接信号槽 ==========
    connect(m_addBtn, &QPushButton::clicked, this, &DriverArchiveView::onAddPerson);
    connect(m_editBtn, &QPushButton::clicked, this, &DriverArchiveView::onEditPerson);
    connect(m_deleteBtn, &QPushButton::clicked, this, &DriverArchiveView::onDeletePerson);
    connect(m_searchBtn, &QPushButton::clicked, this, &DriverArchiveView::onSearchPersons);
    connect(m_refreshBtn, &QPushButton::clicked, this, &DriverArchiveView::onRefreshTable);
    connect(m_searchEdit, &QLineEdit::returnPressed, this, &DriverArchiveView::onSearchPersons);
    connect(m_tableView->selectionModel(),
            &QItemSelectionModel::selectionChanged,
            this,
            &DriverArchiveView::onTableSelectionChanged);
    // 连接信号槽
       connect(m_batchImportBtn, &QPushButton::clicked, this, &DriverArchiveView::onBatchImport);

}

void DriverArchiveView::setController(DriverArchiveController *controller)
{
    m_controller = controller;
    if (m_controller) {
        connect(m_controller, &DriverArchiveController::dataChanged,
                this, &DriverArchiveView::onRefreshTable);
        onRefreshTable();
    }
}

void DriverArchiveView::setFaceRecognizer(FaceRecognizer *recognizer)
{
    m_faceRecognizer = recognizer;
}

void DriverArchiveView::setCameraIndex(int index)
{
    m_cameraIndex = index;
}

void DriverArchiveView::setBeforeCaptureCallback(const std::function<void()> &cb)
{
    m_beforeCapture = cb;
}

// ==================== 加载表格数据 ====================
void DriverArchiveView::loadTableData(const QList<QVariantMap> &data)
{
    m_tableModel->removeRows(0, m_tableModel->rowCount());

    for (const auto &row : data) {
        QList<QStandardItem *> items;
        items.append(new QStandardItem(QString::number(row["id"].toInt())));
        items.append(new QStandardItem(row["name"].toString()));
        items.append(new QStandardItem(row["gender"].toString()));
        items.append(new QStandardItem(row["department"].toString()));
        items.append(new QStandardItem(row["position"].toString()));
        items.append(new QStandardItem(row["phone"].toString()));
        items.append(new QStandardItem(row["status"].toString()));
        items.append(new QStandardItem(row["remarks"].toString()));

        // 设置每行数据
        m_tableModel->appendRow(items);
    }

    m_tableView->resizeColumnsToContents();
}

// ==================== 槽函数实现 ====================

void DriverArchiveView::onAddPerson()
{
    if (!m_controller) {
        QMessageBox::warning(this, "错误", "控制器未初始化");
        return;
    }

    // 弹出添加驾驶员对话框
    DriverAddDialog dialog(this);
    dialog.setFaceRecognizer(m_faceRecognizer);
    dialog.setCameraIndex(m_cameraIndex);
    dialog.setBeforeCaptureCallback(m_beforeCapture);
    dialog.setWindowTitle(QStringLiteral("注册驾驶员"));

    // 如果对话框确认（用户点击了"确定"）
    if (dialog.exec() == QDialog::Accepted) {
        QVariantMap data = dialog.getPersonData();
        data["face_feature"] = dialog.getFaceFeature();

        QString errorMsg;
        if (m_controller->addPerson(data, errorMsg)) {
            QMessageBox::information(this, QStringLiteral("成功"), QStringLiteral("驾驶员注册成功！"));
            onRefreshTable(); // 刷新列表
        } else {
            QMessageBox::warning(this, "失败", errorMsg);
        }
    }
}

void DriverArchiveView::onEditPerson()
{
    if (!m_controller) {
        QMessageBox::warning(this, "错误", "控制器未初始化");
        return;
    }

    // 获取当前选中的行
    QModelIndexList selected = m_tableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择要编辑的驾驶员"));
        return;
    }

    int row = selected.first().row();
    int personId = m_tableModel->item(row, 0)->text().toInt();

    // 获取该驾驶员详细信息
    QVariantMap personData = m_controller->getPersonById(personId);
    if (personData.isEmpty()) {
        QMessageBox::warning(this, "错误", "未找到该驾驶员信息");
        return;
    }

    // 弹出编辑对话框
    DriverAddDialog dialog(this);
    dialog.setFaceRecognizer(m_faceRecognizer);
    dialog.setCameraIndex(m_cameraIndex);
    dialog.setBeforeCaptureCallback(m_beforeCapture);
    dialog.setWindowTitle(QStringLiteral("编辑驾驶员"));
    dialog.setPersonData(personData);

    if (dialog.exec() == QDialog::Accepted) {

        QString errorMsg;
        // 1. 获取普通信息（不包含特征）
            QVariantMap newData = dialog.getPersonData();
            if (m_controller->updatePerson(personId, newData, errorMsg)) {
                // 2. 如果用户上传了新人脸，单独更新特征
                QByteArray newFeature = dialog.getFaceFeature();
                if (!newFeature.isEmpty()) {
                    m_controller->updateFaceFeature(personId, newFeature, errorMsg);
                }
                QMessageBox::information(this, "成功", "驾驶员信息已更新！");
                onRefreshTable();
            } else {
                QMessageBox::warning(this, "失败", errorMsg);
            }
    }
}

void DriverArchiveView::onDeletePerson()
{
    if (!m_controller) {
        QMessageBox::warning(this, "错误", "控制器未初始化");
        return;
    }

    QModelIndexList selected = m_tableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择要删除的驾驶员");
        return;
    }

    int row = selected.first().row();
    int personId = m_tableModel->item(row, 0)->text().toInt();
    QString personName = m_tableModel->item(row, 1)->text();

    // 确认删除
    int result = QMessageBox::question(this,
                                       "确认删除",
                                       QString("确定要删除驾驶员 '%1' 吗？此操作不可撤销！").arg(personName),
                                       QMessageBox::Yes | QMessageBox::No);

    if (result != QMessageBox::Yes) {
        return;
    }

    QString errorMsg;
    if (m_controller->deletePerson(personId, errorMsg)) {
        QMessageBox::information(this, "成功", "驾驶员已删除！");
        onRefreshTable();
    } else {
        QMessageBox::warning(this, "失败", errorMsg);
    }
}

void DriverArchiveView::onSearchPersons()
{
    if (!m_controller) return;

    QString keyword = m_searchEdit->text().trimmed();
    QList<QVariantMap> data;

    if (keyword.isEmpty()) {
        data = m_controller->getAllPersons();
    } else {
        data = m_controller->searchPersonsByName(keyword);
    }

    loadTableData(data);

    if (data.isEmpty() && !keyword.isEmpty()) {
        showMessage(QString("未找到包含 '%1' 的驾驶员").arg(keyword), true);
    } else {
        showMessage(QString("共 %1 条记录").arg(data.size()));
    }
}

void DriverArchiveView::onRefreshTable()
{
    if (!m_controller) {
        return;
    }
    loadTableData(m_controller->getAllPersons());
}

void DriverArchiveView::onTableSelectionChanged()
{
    bool hasSelection = !m_tableView->selectionModel()->selectedRows().isEmpty();
    m_editBtn->setEnabled(hasSelection);
    m_deleteBtn->setEnabled(hasSelection);
}

// ==================== 辅助方法 ====================
void DriverArchiveView::showMessage(const QString &msg, bool isError)
{
    if (isError) {
        qWarning() << QStringLiteral("【档案】") << msg;
    }
}


void DriverArchiveView::onBatchImport()
{
    if (!m_controller) {
        QMessageBox::warning(this, "错误", "控制器未初始化");
        return;
    }

    // 1. 选择 CSV 文件
    QString csvPath = QFileDialog::getOpenFileName(
        this,
        "选择 CSV 文件",
        QDir::homePath(),
        "CSV 文件 (*.csv);;所有文件 (*)"
    );
    if (csvPath.isEmpty()) return;

    // 2. 选择照片文件夹
    QString photoDir = QFileDialog::getExistingDirectory(
        this,
        "选择照片文件夹",
        QDir::homePath()
    );
    if (photoDir.isEmpty()) return;

    // 3. 确认操作
    int ret = QMessageBox::question(
        this,
        "确认批量导入",
        "将导入 CSV 文件中的员工信息，并从照片文件夹中提取人脸特征。\n\n"
        "CSV 文件：" + csvPath + "\n"
        "照片文件夹：" + photoDir + "\n\n"
        "确定继续吗？",
        QMessageBox::Yes | QMessageBox::No
    );
    if (ret != QMessageBox::Yes) return;

    // 4. 执行导入（可以显示进度对话框）
    QProgressDialog progress("正在批量导入员工...", "取消", 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);
    progress.setValue(50);

    // 由于导入是同步操作，这里简单处理
    // 如果员工较多，建议放到子线程中执行
    auto result = m_controller->batchImportPersons(csvPath, photoDir);

    progress.setValue(100);

    // 5. 显示结果
    QString msg = QString(
        "批量导入完成！\n\n"
        "总记录数：%1\n"
        "成功：%2\n"
        "失败：%3\n"
    ).arg(result.total).arg(result.success).arg(result.failed);

    if (result.errors.isEmpty()) {
        QMessageBox::information(this, "导入完成", msg);
    } else {
        QString errorDetail = result.errors.join("\n");
        // 最多显示前 10 条错误，避免弹窗过长
        if (result.errors.size() > 10) {
            errorDetail = result.errors.mid(0, 10).join("\n") +
                          QString("\n... 还有 %1 条错误").arg(result.errors.size() - 10);
        }
        QMessageBox::warning(this, "导入完成（有错误）", msg + "\n\n错误详情：\n" + errorDetail);
    }

    // 刷新表格
    onRefreshTable();
}
