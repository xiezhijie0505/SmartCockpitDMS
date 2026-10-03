#ifndef FATIGUE_IPC_H
#define FATIGUE_IPC_H

#include <cstdint>

// 疲劳事件通道（与帧 shm 分离）
// - 有 QtDBus：还可发 D-Bus 信号
// - 无 QtDBus：Unix 域套接字发 FatigueEvent（12 字节）
static constexpr const char kFatigueSockPath[] = "/tmp/dms_fatigue.sock";

#pragma pack(push, 1)
struct FatigueEvent {
    int32_t level = 0;
    int32_t headDown = 0;
    int32_t eyeClosed = 0;
};
#pragma pack(pop)

static_assert(sizeof(FatigueEvent) == 12, "FatigueEvent size");

#endif // FATIGUE_IPC_H
