#include "ipc/fatigue_level_tx.h"
#include "ipc/fatigue_ipc.h"

#if defined(__linux__)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>
#endif

bool sendFatigueEventToHmi(int level, int headDown, int eyeClosed)
{
#if defined(__linux__)
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return false;
    }

    sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, kFatigueSockPath, sizeof(addr.sun_path) - 1);

    if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        ::close(fd);
        return false;
    }

    FatigueEvent ev;
    ev.level = static_cast<int32_t>(level);
    ev.headDown = headDown ? 1 : 0;
    ev.eyeClosed = eyeClosed ? 1 : 0;

    const ssize_t n = ::send(fd, &ev, sizeof(ev), MSG_NOSIGNAL);
    ::close(fd);
    return n == static_cast<ssize_t>(sizeof(ev));
#else
    (void)level;
    (void)headDown;
    (void)eyeClosed;
    return false;
#endif
}
