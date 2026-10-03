#ifndef FATIGUE_CAN_TX_H
#define FATIGUE_CAN_TX_H

// RK → STM32：经 SocketCAN 发送疲劳告警帧（ID 0x210）
// iface 默认 can1（粤嵌多数是 CAN1）；若板子是 can0，调用时传入 "can0"
bool sendFatigueCanToMcu(int level, const char *iface = "can1");

#endif
