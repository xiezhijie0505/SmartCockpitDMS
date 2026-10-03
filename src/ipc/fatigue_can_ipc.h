#ifndef FATIGUE_CAN_IPC_H
#define FATIGUE_CAN_IPC_H

#include <cstdint>

// 与 STM32 can_protocol.h 保持一致
static constexpr uint32_t kCanIdFatigueCmd = 0x210u;
static constexpr uint8_t  kCanDlcFatigueCmd = 2u;

// data[0] = level (0/1/2)
// data[1] = beep  (1=响, 0=停)

#endif
