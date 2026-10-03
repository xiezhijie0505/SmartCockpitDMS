#include "settingsmodel.h"
#include "databasemanager.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>

const QString SettingsModel::KEY_WORK_START = "work_start_time";
const QString SettingsModel::KEY_WORK_END = "work_end_time";
const QString SettingsModel::KEY_LATE_THRESHOLD = "late_threshold";
const QString SettingsModel::KEY_EARLY_LEAVE_THRESHOLD = "early_leave_threshold";
const QString SettingsModel::KEY_CAMERA_INDEX = "camera_index";

SettingsModel::SettingsModel() {}

// ========== 读取 ==========
QVariant SettingsModel::getValue(const QString &key, const QVariant &defaultValue) const
{
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开，返回默认值";
        return defaultValue;
    }

    QSqlQuery query(db);
    query.prepare("SELECT value FROM system_settings WHERE key = :key");
    query.bindValue(":key", key);

    if (!query.exec()) {
        qDebug() << "查询配置失败：" << query.lastError().text();
        return defaultValue;
    }

    if (query.next()) {
        return query.value("value");
    }
    return defaultValue;
}

// ========== 写入 ==========
bool SettingsModel::setValue(const QString &key, const QVariant &value)
{
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    if (!db.isOpen()) {
        qDebug() << "数据库未打开";
        return false;
    }

    // 使用 REPLACE 或 INSERT OR REPLACE
    QSqlQuery query(db);
    query.prepare("REPLACE INTO system_settings (key, value) VALUES (:key, :value)");
    query.bindValue(":key", key);
    query.bindValue(":value", value.toString());

    if (!query.exec()) {
        qDebug() << "写入配置失败：" << query.lastError().text();
        return false;
    }
    return true;
}

// ========== 获取所有设置 ==========
QMap<QString, QString> SettingsModel::getAllSettings() const
{
    QMap<QString, QString> result;
    QSqlDatabase db = DatabaseManager::getInstance().getDatabase();
    if (!db.isOpen()) return result;

    QSqlQuery query(db);
    query.exec("SELECT key, value FROM system_settings");

    while (query.next()) {
        result[query.value("key").toString()] = query.value("value").toString();
    }
    return result;
}

// ========== 其他方法 ==========
bool SettingsModel::saveSettings(const QMap<QString, QString> &settings)
{
    for (auto it = settings.begin(); it != settings.end(); ++it) {
        if (!setValue(it.key(), it.value())) {
            return false;
        }
    }
    return true;
}

bool SettingsModel::deleteSetting(const QString &key)
{
    QString sql = QString("DELETE FROM system_settings WHERE key = '%1'").arg(key);
    return DatabaseManager::getInstance().executeQuery(sql);
}

bool SettingsModel::contains(const QString &key) const
{
    return getValue(key).isValid();
}
