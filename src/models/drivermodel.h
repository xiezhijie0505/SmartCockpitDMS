#ifndef DRIVERMODEL_H
#define DRIVERMODEL_H

#include <QObject>
#include <QVariantMap>
#include <QList>
#include <QByteArray>
#include <QSqlQuery>

// 人员信息结构体（方便传递数据）
struct DriverInfo {
    int id;
    QString name;
    QString gender;
    QString department;
    QString position;
    QString phone;
    QByteArray faceFeature;
    QString status;
    QString remarks;

    // 转为 QVariantMap（用于显示在表格或界面）
    QVariantMap toMap() const {
        QVariantMap map;
        map["id"] = id;
        map["name"] = name;
        map["gender"] = gender;
        map["department"] = department;
        map["position"] = position;
        map["phone"] = phone;
        map["status"] = status;
        map["remarks"] = remarks;
        // 注意：face_feature 不放入常规显示，太大且不可读
        return map;
    }
};

class DriverModel
{
public:
    DriverModel();

    // ---------- 增 ----------
    bool addPerson(const DriverInfo &info);

    // ---------- 删 ----------
    bool deletePerson(int id);

    // ---------- 改 ----------
    bool updatePerson(int id, const QVariantMap &fields);
    bool updatePersonFaceFeature(int id, const QByteArray &faceFeature);

    // ---------- 查 ----------
    // 获取所有人员（不含 face_feature，用于列表显示）
    QList<QVariantMap> getAllPersons();

    // 按 ID 获取单个人（含 face_feature）
    DriverInfo getPersonById(int id);

    // 按姓名模糊搜索
    QList<QVariantMap> findPersonsByName(const QString &keyword);

    // 按部门筛选
    QList<QVariantMap> findPersonsByDepartment(const QString &department);

    // ---------- 人脸匹配专用 ----------
    // 获取所有人脸特征（用于打卡匹配）
    struct FeatureMatchInfo {
        int id;
        QString name;
        QByteArray faceFeature;
    };
    QList<FeatureMatchInfo> getAllFeaturesForMatch();

    // ---------- 工具 ----------
    int getTotalCount();

private:
    // 将 QSqlQuery 当前行转为 DriverInfo
    DriverInfo rowToDriverInfo(const QSqlQuery &query);
};

#endif // DRIVERMODEL_H
