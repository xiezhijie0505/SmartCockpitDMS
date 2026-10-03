
#ifndef SETTINGSMODEL_H
#define SETTINGSMODEL_H

#include <QString>
#include <QVariant>
#include <QMap>

class SettingsModel
{
public:
    SettingsModel();

    // 读取单个设置（返回 QVariant，支持多种类型）
    QVariant getValue(const QString &key, const QVariant &defaultValue = QVariant()) const;

    // 写入单个设置（接受 QVariant，自动转为 QString 存储）
    bool setValue(const QString &key, const QVariant &value);

    // 获取所有设置 (key-value map，所有值都是 QString)
    QMap<QString, QString> getAllSettings() const;

    // 批量保存
    bool saveSettings(const QMap<QString, QString> &settings);

    // 删除设置
    bool deleteSetting(const QString &key);

    // 检查是否存在某个设置
    bool contains(const QString &key) const;

    // 常用设置键（可在此扩展）
    static const QString KEY_WORK_START;
    static const QString KEY_WORK_END;
    static const QString KEY_LATE_THRESHOLD;
    static const QString KEY_EARLY_LEAVE_THRESHOLD;
    static const QString KEY_CAMERA_INDEX;
};

#endif // SETTINGSMODEL_H
