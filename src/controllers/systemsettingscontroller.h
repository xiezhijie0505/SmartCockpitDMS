#ifndef SYSTEMSETTINGSCONTROLLER_H
#define SYSTEMSETTINGSCONTROLLER_H

#include <QObject>
#include <QTime>
#include <QDateTime>
#include <QString>
#include <QVariantMap>

// 前置声明
class SettingsModel;

class SystemSettingsController : public QObject
{
    Q_OBJECT

public:
    explicit SystemSettingsController(QObject *parent = nullptr);

    // 设置依赖
    void setModel(SettingsModel *model);

    // ==================== 核心配置接口 ====================

    // 上班时间
    QTime getWorkStartTime() const;
    bool setWorkStartTime(const QTime &time);

    // 下班时间
    QTime getWorkEndTime() const;
    bool setWorkEndTime(const QTime &time);

    // 迟到阈值（分钟），例如 30 表示上班后 30 分钟内算迟到
    int getLateThreshold() const;
    bool setLateThreshold(int minutes);

    // 早退阈值（分钟），例如 30 表示下班前 30 分钟内算早退
    int getEarlyLeaveThreshold() const;
    bool setEarlyLeaveThreshold(int minutes);

    // 摄像头索引（0=默认摄像头，1=外接等）
    int getCameraIndex() const;
    bool setCameraIndex(int index);

    // 人脸识别相似度阈值（默认 0.363）
    double getFaceMatchThreshold() const;
    bool setFaceMatchThreshold(double threshold);

    // 系统时钟（识别/告警时间依赖板端系统时间）
    bool setSystemDateTime(const QDateTime &dateTime, QString *errorMsg = nullptr);
    bool syncSystemTimeFromNtp(QString *errorMsg = nullptr);

    // ==================== 通用配置接口 ====================

    // 获取任意配置项（返回 QVariant，需调用者转换）
    QVariant getConfig(const QString &key, const QVariant &defaultValue = QVariant()) const;

    // 设置任意配置项
    bool setConfig(const QString &key, const QVariant &value);

    // 获取所有配置（用于界面显示）
    QMap<QString, QString> getAllConfigs();

    // ==================== 工具方法 ====================

    // 根据当前时间判断打卡状态
    QString getClockInStatus(const QTime &currentTime) const;
    QString getClockOutStatus(const QTime &currentTime) const;

    // 重置所有配置为默认值
    void resetToDefaults();

signals:
    // 配置变更信号（界面可据此刷新显示）
    void configChanged(const QString &key, const QVariant &value);
    void workTimeChanged(const QTime &start, const QTime &end);
    void settingsReset();

private:
    SettingsModel *m_model;

    // 获取配置的辅助方法（自动处理类型转换）
    template<typename T>
    T getConfigValue(const QString &key, const T &defaultValue) const;

    // 设置默认配置（首次启动时调用）
    void initDefaultSettings();

    // 常量配置键名
    static const QString KEY_WORK_START;
    static const QString KEY_WORK_END;
    static const QString KEY_LATE_THRESHOLD;
    static const QString KEY_EARLY_LEAVE_THRESHOLD;
    static const QString KEY_CAMERA_INDEX;
    static const QString KEY_FACE_MATCH_THRESHOLD;
};

#endif // SYSTEMSETTINGSCONTROLLER_H
