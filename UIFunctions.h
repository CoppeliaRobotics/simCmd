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
    void onExecCode(QString code, int scriptHandleOrType, QString scriptName);

signals:
    void scriptListChanged(QMap<int,QString> childScripts, QMap<int,QString> jointCtrlCallbacks, QMap<int,QString> customizationScripts);
};

#endif // UIFUNCTIONS_H_INCLUDED

