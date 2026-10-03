#ifndef DRIVERARCHIVECONTROLLER_H
#define DRIVERARCHIVECONTROLLER_H

#include <QObject>
#include <QVariantMap>
#include <QList>
#include <QByteArray>
#include "algorithms/facerecognizer.h"   // 添加这一行
#include <QFile>          // ← 新增：文件操作
#include <QTextStream>    // ← 新增：文本流读写
#include <QIODevice>      // ← 新增：IO 设备标志
#include <QDir>           // ← 新增：目录操作（可能需要）
// 前置声明
class DriverModel;
class FaceRecognizer;   // 前置声明




class DriverArchiveController : public QObject
{
    Q_OBJECT

public:
    explicit DriverArchiveController(QObject *parent = nullptr);

    // 批量导入：从 CSV 文件和照片文件夹批量添加员工
    struct ImportResult {
        int total;          // 总记录数
        int success;        // 成功数
        int failed;         // 失败数
        QStringList errors; // 错误信息列表
    };

    // 设置依赖（注入 DriverModel）
    void setModel(DriverModel *model);

    // ========== 业务接口（供界面调用） ==========

    // 添加人员（返回是否成功 + 错误信息）
    bool addPerson(const QVariantMap &personData, QString &errorMsg);

    // 删除人员
    bool deletePerson(int id, QString &errorMsg);

    // 更新人员信息（支持部分字段更新）
    bool updatePerson(int id, const QVariantMap &fields, QString &errorMsg);

    // 更新人脸特征
    bool updateFaceFeature(int id, const QByteArray &faceFeature, QString &errorMsg);

    // ========== 查询接口 ==========

    // 获取所有人员（用于表格显示）
    QList<QVariantMap> getAllPersons();

    // 按 ID 查询
    QVariantMap getPersonById(int id);

    // 按姓名搜索（模糊匹配）
    QList<QVariantMap> searchPersonsByName(const QString &keyword);

    // 按部门筛选
    QList<QVariantMap> getPersonsByDepartment(const QString &department);

    // 获取总人数
    int getTotalCount();

    // 检查姓名是否已存在（用于添加时去重）
    bool isNameExists(const QString &name, int excludeId = -1);

    // 获取所有人员的特征（供打卡匹配使用）
    QList<QVariantMap> getAllFeaturesForMatch();

    ImportResult batchImportPersons(const QString &csvFilePath, const QString &photoDirPath);
    // 新增：设置人脸识别器
       void setFaceRecognizer(FaceRecognizer *faceRecognizer);

signals:
    // 数据变更信号（界面可据此刷新列表）
    void dataChanged();
    void personAdded(int id, const QString &name);
    void personDeleted(int id);
    void personUpdated(int id);

private:
    DriverModel *m_model;

    // 辅助：校验人员数据合法性
    bool validatePersonData(const QVariantMap &data, QString &errorMsg);
      FaceRecognizer *m_faceRecognizer;   // ← 新增成员变量
};

#endif // DRIVERARCHIVECONTROLLER_H
