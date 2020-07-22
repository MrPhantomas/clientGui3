#ifndef CLIENTSTRUCT_H
#define CLIENTSTRUCT_H
#include <QDateTime>
#endif // CLIENTSTRUCT_H

struct TInfo
{
    quint8 t = 0;
    QDateTime lastMessTime;
};

struct DfInfo
{
    quint64 emptyDfSize = 0;
    QDateTime lastMessTime;
};

struct PoInfo
{
    QStringList poLst;
    QDateTime lastMessTime;
};
