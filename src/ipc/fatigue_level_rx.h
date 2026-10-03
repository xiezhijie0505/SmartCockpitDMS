#ifndef FATIGUE_LEVEL_RX_H
#define FATIGUE_LEVEL_RX_H

#include <QObject>

class QSocketNotifier;

// HMI：监听 /tmp/dms_fatigue.sock，收到 FatigueEvent 后发信号
class FatigueLevelSocketServer : public QObject
{
    Q_OBJECT
public:
    explicit FatigueLevelSocketServer(QObject *parent = nullptr);
    ~FatigueLevelSocketServer() override;

    bool start();
    void stop();

signals:
    void levelReceived(int level); // 兼容
    void fatigueReceived(int level, int headDown, int eyeClosed);

private slots:
    void onListenActivated();
    void onConnActivated();

private:
    int m_listenFd = -1;
    int m_connFd = -1;
    QSocketNotifier *m_listenNotifier = nullptr;
    QSocketNotifier *m_connNotifier = nullptr;
};

#endif
