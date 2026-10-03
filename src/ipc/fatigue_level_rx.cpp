#include "ipc/fatigue_level_rx.h"
#include "ipc/fatigue_ipc.h"

#include <QSocketNotifier>
#include <QDebug>
#include <QtGlobal>

#ifdef Q_OS_LINUX
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#endif

FatigueLevelSocketServer::FatigueLevelSocketServer(QObject *parent)
    : QObject(parent)
{
}

FatigueLevelSocketServer::~FatigueLevelSocketServer()
{
    stop();
}

bool FatigueLevelSocketServer::start()
{
#ifdef Q_OS_LINUX
    stop();

    ::unlink(kFatigueSockPath);

    m_listenFd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_listenFd < 0) {
        qWarning() << QStringLiteral("【疲劳通道】创建套接字失败");
        return false;
    }

    sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, kFatigueSockPath, sizeof(addr.sun_path) - 1);

    if (::bind(m_listenFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        qWarning() << QStringLiteral("【疲劳通道】绑定失败，请确认 /tmp 可写");
        ::close(m_listenFd);
        m_listenFd = -1;
        return false;
    }

    if (::listen(m_listenFd, 4) != 0) {
        qWarning() << QStringLiteral("【疲劳通道】监听失败");
        stop();
        return false;
    }

    m_listenNotifier = new QSocketNotifier(m_listenFd, QSocketNotifier::Read, this);
    connect(m_listenNotifier, &QSocketNotifier::activated,
            this, &FatigueLevelSocketServer::onListenActivated);

    qInfo() << QStringLiteral("【疲劳通道】已就绪，等待 dms_ai 推送");
    return true;
#else
    return false;
#endif
}

void FatigueLevelSocketServer::stop()
{
#ifdef Q_OS_LINUX
    if (m_connNotifier) {
        m_connNotifier->setEnabled(false);
        delete m_connNotifier;
        m_connNotifier = nullptr;
    }
    if (m_listenNotifier) {
        m_listenNotifier->setEnabled(false);
        delete m_listenNotifier;
        m_listenNotifier = nullptr;
    }
    if (m_connFd >= 0) {
        ::close(m_connFd);
        m_connFd = -1;
    }
    if (m_listenFd >= 0) {
        ::close(m_listenFd);
        m_listenFd = -1;
    }
    ::unlink(kFatigueSockPath);
#endif
}

void FatigueLevelSocketServer::onListenActivated()
{
#ifdef Q_OS_LINUX
    if (m_listenFd < 0) {
        return;
    }

    const int fd = ::accept(m_listenFd, nullptr, nullptr);
    if (fd < 0) {
        return;
    }

    if (m_connNotifier) {
        m_connNotifier->setEnabled(false);
        delete m_connNotifier;
        m_connNotifier = nullptr;
    }
    if (m_connFd >= 0) {
        ::close(m_connFd);
    }
    m_connFd = fd;

    m_connNotifier = new QSocketNotifier(m_connFd, QSocketNotifier::Read, this);
    connect(m_connNotifier, &QSocketNotifier::activated,
            this, &FatigueLevelSocketServer::onConnActivated);
#endif
}

void FatigueLevelSocketServer::onConnActivated()
{
#ifdef Q_OS_LINUX
    if (m_connFd < 0) {
        return;
    }

    FatigueEvent ev;
    const ssize_t n = ::recv(m_connFd, &ev, sizeof(ev), MSG_WAITALL);
    if (n == static_cast<ssize_t>(sizeof(ev))) {
        emit fatigueReceived(static_cast<int>(ev.level),
                             static_cast<int>(ev.headDown),
                             static_cast<int>(ev.eyeClosed));
        emit levelReceived(static_cast<int>(ev.level));
    }

    if (m_connNotifier) {
        m_connNotifier->setEnabled(false);
        delete m_connNotifier;
        m_connNotifier = nullptr;
    }
    ::close(m_connFd);
    m_connFd = -1;
#endif
}
