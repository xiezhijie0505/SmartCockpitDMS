#ifndef FATIGUE_LEVEL_TX_H
#define FATIGUE_LEVEL_TX_H

// dms_ai → HMI：经 Unix socket 发送 FatigueEvent
bool sendFatigueEventToHmi(int level, int headDown, int eyeClosed);

// 兼容旧调用
inline bool sendFatigueLevelToHmi(int level)
{
    return sendFatigueEventToHmi(level, 0, 0);
}

#endif
