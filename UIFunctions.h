#ifndef UIFUNCTIONS_H_INCLUDED
#define UIFUNCTIONS_H_INCLUDED

#include "config.h"

#include <QObject>
#include <QString>

#include "stubs.h"

class UIFunctions : public QObject
{
    Q_OBJECT

public:
    virtual ~UIFunctions();

    static UIFunctions * getInstance(QObject *parent = 0);
    static void destroyInstance();

    void connectSignals();

private:
    UIFunctions(QObject *parent = 0);

    static UIFunctions *instance;

public slots:
    void onExecCode(QString code);

signals:
};

#endif // UIFUNCTIONS_H_INCLUDED

