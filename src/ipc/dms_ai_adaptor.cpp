#include "ipc/dms_ai_adaptor.h"

DmsAiAdaptor::DmsAiAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent)
{
    setAutoRelaySignals(true);
}

QString DmsAiAdaptor::ping()
{
    return QStringLiteral("pong");
}
