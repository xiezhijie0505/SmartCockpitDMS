#include "systemsettingscontroller.h"
#include "models/settingsmodel.h"
#include <QDebug>
#include <QProcess>

#ifdef Q_OS_LINUX
#include <sys/time.h>
#include <errno.h>
#include <ctime>
#include <cstring>
#endif

// ==================== 静态常量定义 ====================
const QString SystemSettingsController::KEY_WORK_START = "work_start_time";
const QString SystemSettingsController::KEY_WORK_END = "work_end_time";
const QString SystemSettingsController::KEY_LATE_THRESHOLD = "late_threshold";
const QString SystemSettingsController::KEY_EARLY_LEAVE_THRESHOLD = "early_leave_threshold";
const QString SystemSettingsController::KEY_CAMERA_INDEX = "camera_index";
const QString SystemSettingsController::KEY_FACE_MATCH_THRESHOLD = "face_match_threshold";

// ==================== 构造与初始化 ====================
SystemSettingsController::SystemSettingsController(QObject *parent)
    : QObject(parent)
    , m_model(nullptr)
{
}

void SystemSettingsController::setModel(SettingsModel *model)
{
    m_model = model;
    if (m_model) {
        // 首次启动时初始化默认配置
        initDefaultSettings();
    }
}

void SystemSettingsController::initDefaultSettings()
{
    QVariant val;

    val = m_model->getValue(KEY_WORK_START);
    if (!val.isValid()) {
        setConfig(KEY_WORK_START, "09:00");
    }

    val = m_model->getValue(KEY_WORK_END);
    if (!val.isValid()) {
        setConfig(KEY_WORK_END, "18:00");
    }

    val = m_model->getValue(KEY_LATE_THRESHOLD);
    if (!val.isValid()) {
        setConfig(KEY_LATE_THRESHOLD, 30);
    }

    val = m_model->getValue(KEY_EARLY_LEAVE_THRESHOLD);
    if (!val.isValid()) {
        setConfig(KEY_EARLY_LEAVE_THRESHOLD, 30);
    }

    val = m_model->getValue(KEY_CAMERA_INDEX);
    if (!val.isValid()) {
        setConfig(KEY_CAMERA_INDEX, 0);
    }

    val = m_model->getValue(KEY_FACE_MATCH_THRESHOLD);
    if (!val.isValid()) {
        setConfig(KEY_FACE_MATCH_THRESHOLD, 0.363);
    }
}

// ==================== 核心配置实现 ====================

QTime SystemSettingsController::getWorkStartTime() const
{
    QString timeStr = getConfig(KEY_WORK_START, "09:00").toString();
    return QTime::fromString(timeStr, "HH:mm");
}

bool SystemSettingsController::setWorkStartTime(const QTime &time)
{
    if (!time.isValid()) return false;
    QString timeStr = time.toString("HH:mm");
    bool ok = setConfig(KEY_WORK_START, timeStr);
    if (ok) {
        emit configChanged(KEY_WORK_START, timeStr);
        emit workTimeChanged(getWorkStartTime(), getWorkEndTime());
    }
    return ok;
}

QTime SystemSettingsController::getWorkEndTime() const
{
    QString timeStr = getConfig(KEY_WORK_END, "18:00").toString();
    return QTime::fromString(timeStr, "HH:mm");
}

bool SystemSettingsController::setWorkEndTime(const QTime &time)
{
    if (!time.isValid()) return false;
    // 下班时间不能早于上班时间
    if (time <= getWorkStartTime()) {
        qDebug() << "下班时间必须晚于上班时间";
        return false;
    }
    QString timeStr = time.toString("HH:mm");
    bool ok = setConfig(KEY_WORK_END, timeStr);
    if (ok) {
        emit configChanged(KEY_WORK_END, timeStr);
        emit workTimeChanged(getWorkStartTime(), getWorkEndTime());
    }
    return ok;
}

int SystemSettingsController::getLateThreshold() const
{
    return getConfig(KEY_LATE_THRESHOLD, 30).toInt();
}

bool SystemSettingsController::setLateThreshold(int minutes)
{
    if (minutes < 0 || minutes > 120) {
        qDebug() << "迟到阈值应在 0~120 分钟之间";
        return false;
    }
    bool ok = setConfig(KEY_LATE_THRESHOLD, minutes);
    if (ok) {
        emit configChanged(KEY_LATE_THRESHOLD, minutes);
    }
    return ok;
}

int SystemSettingsController::getEarlyLeaveThreshold() const
{
    return getConfig(KEY_EARLY_LEAVE_THRESHOLD, 30).toInt();
}

bool SystemSettingsController::setEarlyLeaveThreshold(int minutes)
{
    if (minutes < 0 || minutes > 120) {
        qDebug() << "早退阈值应在 0~120 分钟之间";
        return false;
    }
    bool ok = setConfig(KEY_EARLY_LEAVE_THRESHOLD, minutes);
    if (ok) {
        emit configChanged(KEY_EARLY_LEAVE_THRESHOLD, minutes);
    }
    return ok;
}

int SystemSettingsController::getCameraIndex() const
{
    return getConfig(KEY_CAMERA_INDEX, 0).toInt();
}

bool SystemSettingsController::setCameraIndex(int index)
{
    if (index < 0) {
        qDebug() << "摄像头索引不能为负数";
        return false;
    }
    bool ok = setConfig(KEY_CAMERA_INDEX, index);
    if (ok) {
        emit configChanged(KEY_CAMERA_INDEX, index);
    }
    return ok;
}

double SystemSettingsController::getFaceMatchThreshold() const
{
    return getConfig(KEY_FACE_MATCH_THRESHOLD, 0.363).toDouble();
}

bool SystemSettingsController::setFaceMatchThreshold(double threshold)
{
    if (threshold < 0.0 || threshold > 1.0) {
        qDebug() << "阈值应在 0.0 ~ 1.0 之间";
        return false;
    }
    bool ok = setConfig(KEY_FACE_MATCH_THRESHOLD, threshold);
    if (ok) {
        emit configChanged(KEY_FACE_MATCH_THRESHOLD, threshold);
    }
    return ok;
}

bool SystemSettingsController::setSystemDateTime(const QDateTime &dateTime, QString *errorMsg)
{
    if (!dateTime.isValid()) {
        if (errorMsg) {
            *errorMsg = QStringLiteral("无效的日期时间");
        }
        return false;
    }

#ifdef Q_OS_LINUX
    const QDateTime local = dateTime.toLocalTime();
    const QString stamp = local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));

    // 1) 优先 settimeofday
    struct timeval tv;
    tv.tv_sec = static_cast<time_t>(local.toSecsSinceEpoch());
    tv.tv_usec = 0;
    if (settimeofday(&tv, nullptr) == 0) {
        QProcess::execute(QStringLiteral("hwclock"), QStringList() << QStringLiteral("-w"));
        return true;
    }
    const int firstErrno = errno;

    // 2) 回退：与串口相同的 date -s（BusyBox 常见）
    const QList<QPair<QString, QStringList>> cmds = {
        {QStringLiteral("date"), {QStringLiteral("-s"), stamp}},
        {QStringLiteral("busybox"), {QStringLiteral("date"), QStringLiteral("-s"), stamp}},
    };
    for (const auto &cmd : cmds) {
        QProcess proc;
        proc.start(cmd.first, cmd.second);
        if (!proc.waitForStarted(2000)) {
            continue;
        }
        if (!proc.waitForFinished(5000)) {
            proc.kill();
            continue;
        }
        if (proc.exitCode() == 0) {
            QProcess::execute(QStringLiteral("hwclock"), QStringList() << QStringLiteral("-w"));
            return true;
        }
    }

    if (errorMsg) {
        *errorMsg = QStringLiteral("设置失败：settimeofday errno=%1 (%2)。请确认用 root 运行，或串口执行: date -s \"%3\"")
                        .arg(firstErrno)
                        .arg(QString::fromLocal8Bit(strerror(firstErrno)))
                        .arg(stamp);
    }
    return false;
#else
    Q_UNUSED(dateTime);
    if (errorMsg) {
        *errorMsg = QStringLiteral("仅 Linux 板端支持设置系统时间");
    }
    return false;
#endif
}

bool SystemSettingsController::syncSystemTimeFromNtp(QString *errorMsg)
{
#ifdef Q_OS_LINUX
    // 依次尝试常见 BusyBox / ntpdate 命令（板端未必都有）
    const QList<QPair<QString, QStringList>> cmds = {
        {QStringLiteral("busybox"), {QStringLiteral("ntpd"), QStringLiteral("-q"), QStringLiteral("-n"), QStringLiteral("-p"), QStringLiteral("ntp.aliyun.com")}},
        {QStringLiteral("ntpd"), {QStringLiteral("-q"), QStringLiteral("-n"), QStringLiteral("-p"), QStringLiteral("ntp.aliyun.com")}},
        {QStringLiteral("ntpdate"), {QStringLiteral("-u"), QStringLiteral("ntp.aliyun.com")}},
        {QStringLiteral("busybox"), {QStringLiteral("ntpdate"), QStringLiteral("-u"), QStringLiteral("ntp.aliyun.com")}},
    };

    for (const auto &cmd : cmds) {
        QProcess proc;
        proc.start(cmd.first, cmd.second);
        if (!proc.waitForStarted(2000)) {
            continue;
        }
        if (!proc.waitForFinished(15000)) {
            proc.kill();
            continue;
        }
        if (proc.exitCode() == 0) {
            QProcess::execute(QStringLiteral("hwclock"), QStringList() << QStringLiteral("-w"));
            return true;
        }
    }

    if (errorMsg) {
        *errorMsg = QStringLiteral("NTP 对时失败：请检查网络，或改用手动设置时间");
    }
    return false;
#else
    if (errorMsg) {
        *errorMsg = QStringLiteral("仅 Linux 板端支持 NTP 对时");
    }
    return false;
#endif
}

// ==================== 通用配置接口 ====================

QVariant SystemSettingsController::getConfig(const QString &key, const QVariant &defaultValue) const
{
    if (!m_model) return defaultValue;
    return m_model->getValue(key, defaultValue);
}

bool SystemSettingsController::setConfig(const QString &key, const QVariant &value)
{
    if (!m_model) return false;
    bool ok = m_model->setValue(key, value);
    if (ok) {
        emit configChanged(key, value);
    }
    return ok;
}

QMap<QString, QString> SystemSettingsController::getAllConfigs()
{
    if (!m_model) return QMap<QString, QString>();
    return m_model->getAllSettings();
}

// ==================== 打卡状态判断 ====================

QString SystemSettingsController::getClockInStatus(const QTime &currentTime) const
{
    QTime startTime = getWorkStartTime();
    int lateThreshold = getLateThreshold();

    if (currentTime <= startTime) {
        return QStringLiteral("正常");
    }

    int minutesLate = startTime.secsTo(currentTime) / 60;
    if (minutesLate <= lateThreshold) {
        return QStringLiteral("正常");
    } else {
        return QStringLiteral("迟到");
    }
}

QString SystemSettingsController::getClockOutStatus(const QTime &currentTime) const
{
    QTime endTime = getWorkEndTime();
    int earlyLeaveThreshold = getEarlyLeaveThreshold();

    if (currentTime >= endTime) {
        return QStringLiteral("正常");
    }

    int minutesEarly = currentTime.secsTo(endTime) / 60;
    if (minutesEarly <= earlyLeaveThreshold) {
        return QStringLiteral("正常");
    } else {
        return QStringLiteral("早退");
    }
}

// ==================== 重置 ====================

void SystemSettingsController::resetToDefaults()
{
    setWorkStartTime(QTime(9, 0));
    setWorkEndTime(QTime(18, 0));
    setLateThreshold(30);
    setEarlyLeaveThreshold(30);
    setCameraIndex(0);
    setFaceMatchThreshold(0.363);
    emit settingsReset();
    qDebug() << "系统设置已重置为默认值";
}
