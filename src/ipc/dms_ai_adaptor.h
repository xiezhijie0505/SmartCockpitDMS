#ifndef DMS_AI_ADAPTOR_H
#define DMS_AI_ADAPTOR_H

#include <QDBusAbstractAdaptor>
#include <QString>

// S5 ③：dms_ai 侧 D-Bus 适配器（发出 FatigueLevelChanged）
class DmsAiAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.gec.dms.AI")

public:
    explicit DmsAiAdaptor(QObject *parent);

public slots:
    QString ping();

signals:
    void FatigueLevelChanged(int level);
};

#endif // DMS_AI_ADAPTOR_H
