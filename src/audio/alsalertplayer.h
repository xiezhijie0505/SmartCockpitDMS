#ifndef ALSAALERTPLAYER_H
#define ALSAALERTPLAYER_H

#include <QObject>
#include <QString>
#include <QElapsedTimer>
#include <QAtomicInt>

// S2：疲劳告警播报（第一版用 aplay，后台线程，带冷却）
class AlsaAlertPlayer : public QObject
{
    Q_OBJECT
public:
    explicit AlsaAlertPlayer(QObject *parent = nullptr);

    void setWavPath(const QString &path);
    void setCooldownMs(int ms);  // 默认 10000
    void play();                 // 非阻塞

private:
    void playBlocking();

    QString m_wavPath;
    int m_cooldownMs = 10000;
    QElapsedTimer m_lastPlay;
    bool m_hasPlayed = false;
    QAtomicInt m_playing;  // 1=正在播
};

#endif // ALSAALERTPLAYER_H
