#include "ipc/fatigue_can_tx.h"
#include "ipc/fatigue_can_ipc.h"

#if defined(__linux__)
#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#endif

bool sendFatigueCanToMcu(int level, const char *iface)
{
#if defined(__linux__)
    if (!iface || !iface[0]) {
        return false;
    }

    const int fd = ::socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (fd < 0) {
        std::perror("socket");
        return false;
    }

    ifreq ifr;
    std::memset(&ifr, 0, sizeof(ifr));
    std::strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);
    if (::ioctl(fd, SIOCGIFINDEX, &ifr) < 0) {
        std::perror("ioctl");
        ::close(fd);
        return false;
    }

    sockaddr_can addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        std::perror("bind");
        ::close(fd);
        return false;
    }

    can_frame frame;
    std::memset(&frame, 0, sizeof(frame));
    frame.can_id = kCanIdFatigueCmd;
    frame.can_dlc = kCanDlcFatigueCmd;
    frame.data[0] = static_cast<__u8>(level < 0 ? 0 : (level > 2 ? 2 : level));
    frame.data[1] = (frame.data[0] >= 1) ? 1 : 0;

    const ssize_t n = ::write(fd, &frame, sizeof(frame));
    ::close(fd);
    return n == static_cast<ssize_t>(sizeof(frame));
#else
    (void)level;
    (void)iface;
    return false;
#endif
}
