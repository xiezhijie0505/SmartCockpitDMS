#include "alsalertplayer.h"

#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QtConcurrent>

AlsaAlertPlayer::AlsaAlertPlayer(QObject *parent)
    : QObject(parent)
    , m_playing(0)
{
}

void AlsaAlertPlayer::setWavPath(const QString &path)
{
    m_wavPath = path;
}

void AlsaAlertPlayer::setCooldownMs(int ms)
{
    m_cooldownMs = ms > 0 ? ms : 10000;
}

void AlsaAlertPlayer::play()
{
    if (m_wavPath.isEmpty()) {
        qWarning() << "[AlsaAlert] wav path empty";
        return;
    }
    if (!QFile::exists(m_wavPath)) {
        qWarning() << "[AlsaAlert] wav not found:" << m_wavPath;
        return;
    }
    // 正在播：不排队
    if (m_playing.loadAcquire() != 0) {
        return;
    }
    // 冷却：距上次成功触发不足 cooldown 则忽略
    if (m_hasPlayed && m_lastPlay.isValid()
        && m_lastPlay.elapsed() < m_cooldownMs) {
        return;
    }
    if (!m_playing.testAndSetAcquire(0, 1)) {
        return;
    }
    m_hasPlayed = true;
    m_lastPlay.restart();

    // 丢到线程池，避免卡住摄像头预览
    QtConcurrent::run([this]() {
        playBlocking();
        m_playing.storeRelease(0);
    });
}

void AlsaAlertPlayer::playBlocking()
{
#ifdef Q_OS_LINUX
    qDebug() << "[AlsaAlert] aplay" << m_wavPath;
    const int code = QProcess::execute(QStringLiteral("aplay"),
                                       QStringList{m_wavPath});
    if (code != 0) {
        qWarning() << "[AlsaAlert] aplay failed, exit=" << code;
    }
#else
    qDebug() << "[AlsaAlert] skip aplay on non-Linux:" << m_wavPath;
#endif
}
