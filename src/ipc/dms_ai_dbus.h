#ifndef DMS_AI_DBUS_H
#define DMS_AI_DBUS_H

// S5：dms_ai ↔ HMI 疲劳事件约定（发端/收端必须用同一套名字）
// 信号：FatigueLevelChanged(int level)  — level: 0 正常 / 1 轻度 / 2 重度

// C++11：用 static constexpr（不要 inline，那是 C++17）
static constexpr const char kDmsAiDbusService[] = "com.gec.dms.AI";
static constexpr const char kDmsAiDbusPath[]    = "/com/gec/dms/AI";
static constexpr const char kDmsAiDbusIface[]   = "com.gec.dms.AI";
static constexpr const char kDmsAiDbusSignal[]  = "FatigueLevelChanged";

#endif // DMS_AI_DBUS_H
