#include "drivermodel.h"
#include "databasemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

DriverModel::DriverModel() {}

// ==================== 增 ====================
bool DriverModel::addPerson(const DriverInfo &info)
{
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return false;
    }

    QSqlQuery query(db);
    query.prepare(R"(
        INSERT INTO personnel (name, gender, department, position, phone, face_feature, status, remarks)
        VALUES (:name, :gender, :department, :position, :phone, :face_feature, :status, :remarks)
    )");
    query.bindValue(":name", info.name);
    query.bindValue(":gender", info.gender);
    query.bindValue(":department", info.department);
    query.bindValue(":position", info.position);
    query.bindValue(":phone", info.phone);
    query.bindValue(":face_feature", info.faceFeature); // QByteArray 自动转为 BLOB
    query.bindValue(":status", info.status.isEmpty() ? "在职" : info.status);
    query.bindValue(":remarks", info.remarks);

    if (!query.exec()) {
        qDebug() << "添加人员失败：" << query.lastError().text();
        return false;
    }
    qDebug() << "添加人员成功：" << info.name;
    return true;
}

// ==================== 删 ====================
bool DriverModel::deletePerson(int id)
{
    QString sql = QString("DELETE FROM personnel WHERE id = %1").arg(id);
    bool ok = DatabaseManager::getInstance().executeQuery(sql);
    if (ok) {
        qDebug() << "删除人员 ID：" << id;
    }
    return ok;
}

// ==================== 改 ====================
bool DriverModel::updatePerson(int id, const QVariantMap &fields)
{
    if (fields.isEmpty()) {
        qDebug() << "没有要更新的字段";
        return false;
    }
    // ⚠️ 防御：如果包含 face_feature 但为空，提示错误
       if (fields.contains("face_feature") && fields["face_feature"].toByteArray().isEmpty()) {
           qDebug() << "【更新人员】人脸特征不能为空，请重新上传图片";
           return false;
       }

    // 构建 SET 子句，如 "name = :name, department = :department"
    QStringList setClauses;
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        setClauses.append(QString("%1 = :%1").arg(it.key()));
    }

    QString sql = QString("UPDATE personnel SET %1 WHERE id = :id")
                      .arg(setClauses.join(", "));

    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    if (!db.isOpen()) return false;

    QSqlQuery query(db);
    query.prepare(sql);
    for (auto it = fields.begin(); it != fields.end(); ++it) {
        query.bindValue(":" + it.key(), it.value());
    }
    query.bindValue(":id", id);

    if (!query.exec()) {
        qDebug() << "更新人员失败：" << query.lastError().text();
        return false;
    }
    qDebug() << "更新人员 ID：" << id;
    return true;
}

bool DriverModel::updatePersonFaceFeature(int id, const QByteArray &faceFeature)
{
    QVariantMap fields;
    fields["face_feature"] = faceFeature;
    return updatePerson(id, fields);
}

// ==================== 查 ====================
QList<QVariantMap> DriverModel::getAllPersons()
{
    QList<QVariantMap> result;
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    if (!db.isOpen()) return result;

    QSqlQuery query(db);
    query.exec("SELECT id, name, gender, department, position, phone, status, remarks FROM personnel ORDER BY id");

    while (query.next()) {
        QVariantMap row;
        row["id"] = query.value("id");
        row["name"] = query.value("name");
        row["gender"] = query.value("gender");
        row["department"] = query.value("department");
        row["position"] = query.value("position");
        row["phone"] = query.value("phone");
        row["status"] = query.value("status");
        row["remarks"] = query.value("remarks");
        result.append(row);
    }
    return result;
}

DriverInfo DriverModel::getPersonById(int id)
{
    DriverInfo info;
    info.id = -1; // 默认无效ID

    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    if (!db.isOpen()) return info;

    QSqlQuery query(db);
    query.prepare("SELECT * FROM personnel WHERE id = :id");
    query.bindValue(":id", id);

    if (!query.exec() || !query.next()) {
        qDebug() << "未找到 ID：" << id;
        return info;
    }

    return rowToDriverInfo(query);
}

QList<QVariantMap> DriverModel::findPersonsByName(const QString &keyword)
{
    QList<QVariantMap> result;
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    if (!db.isOpen()) return result;

    QSqlQuery query(db);
    query.prepare("SELECT id, name, gender, department, position, phone, status, remarks "
                  "FROM personnel WHERE name LIKE :keyword ORDER BY id");
    query.bindValue(":keyword", "%" + keyword + "%");

    if (!query.exec()) return result;

    while (query.next()) {
        QVariantMap row;
        row["id"] = query.value("id");
        row["name"] = query.value("name");
        row["gender"] = query.value("gender");
        row["department"] = query.value("department");
        row["position"] = query.value("position");
        row["phone"] = query.value("phone");
        row["status"] = query.value("status");
        row["remarks"] = query.value("remarks");
        result.append(row);
    }
    return result;
}

QList<QVariantMap> DriverModel::findPersonsByDepartment(const QString &department)
{
    QList<QVariantMap> result;
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    if (!db.isOpen()) return result;

    QSqlQuery query(db);
    query.prepare("SELECT id, name, gender, department, position, phone, status, remarks "
                  "FROM personnel WHERE department = :department ORDER BY id");
    query.bindValue(":department", department);

    if (!query.exec()) return result;

    while (query.next()) {
        QVariantMap row;
        row["id"] = query.value("id");
        row["name"] = query.value("name");
        row["gender"] = query.value("gender");
        row["department"] = query.value("department");
        row["position"] = query.value("position");
        row["phone"] = query.value("phone");
        row["status"] = query.value("status");
        row["remarks"] = query.value("remarks");
        result.append(row);
    }
    return result;
}

// ==================== 人脸匹配专用 ====================
QList<DriverModel::FeatureMatchInfo> DriverModel::getAllFeaturesForMatch()
{
    QList<FeatureMatchInfo> result;
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    if (!db.isOpen()) return result;

    QSqlQuery query(db);
    query.exec("SELECT id, name, face_feature FROM personnel WHERE face_feature IS NOT NULL");

    while (query.next()) {
        FeatureMatchInfo info;
        info.id = query.value("id").toInt();
        info.name = query.value("name").toString();
        info.faceFeature = query.value("face_feature").toByteArray();
        result.append(info);
    }
    return result;
}

// ==================== 工具 ====================
int DriverModel::getTotalCount()
{
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    if (!db.isOpen()) return 0;

    QSqlQuery query(db);
    if (!query.exec("SELECT COUNT(*) FROM personnel")) return 0;
    if (query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

// ==================== 私有辅助 ====================
DriverInfo DriverModel::rowToDriverInfo(const QSqlQuery &query)
{
    DriverInfo info;
    info.id = query.value("id").toInt();
    info.name = query.value("name").toString();
    info.gender = query.value("gender").toString();
    info.department = query.value("department").toString();
    info.position = query.value("position").toString();
    info.phone = query.value("phone").toString();
    info.faceFeature = query.value("face_feature").toByteArray();
    info.status = query.value("status").toString();
    info.remarks = query.value("remarks").toString();
    return info;
}
