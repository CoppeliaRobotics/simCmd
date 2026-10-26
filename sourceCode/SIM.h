#ifndef UIFUNCTIONS_H_INCLUDED
#define UIFUNCTIONS_H_INCLUDED

#include "config.h"
#include <QObject>
#include <QString>
#include <QMap>
#include <simPlusPlus-2/Lib.h>
#include "stubs.h"

class SIM : public QObject
{
    Q_OBJECT

public:
    virtual ~SIM();

    static SIM * getInstance(QObject *parent = 0);
    static void destroyInstance();

    void connectSignals();

    void loadHistory();
    void appendHistory(QString code);

public slots:
    void clearHistory();

private:

    SIM(QObject *parent = 0);

    static SIM *instance;

public slots:
    void addLog(int verbosity, QString message);
    void onExecCode(int scriptHandle, QString code);
    void onAskCompletion(int scriptHandle, QString input, int pos, QStringList *clout);
    void onAskCallTip(int scriptHandle, QString input, int pos);
    void onCurrentScriptChanged(int scriptHandle);

signals:
    void setVisible(bool visible);
    void scriptListChanged(QMap<int,QString> sandboxScripts, QMap<int,QString> sandboxLang, const QString &preferredSandboxLang, int mainScript, QMap<int,QString> simulationScripts, QMap<int,QString> customizationScripts, QMap<int,QString> addons, bool simRunning, bool isRunningJustChanged);
    void setCompletion(QStringList s);
    void setCallTip(QString s);
    void historyChanged(QStringList history);
    void setAutoAcceptCommonCompletionPrefix(bool b);
    void setShowMatchingHistory(bool b);
    void setSelectedScript(int scriptHandle, bool silent, bool fallbackToSandbox);
    void toggleStatusbarHeight();

private:
    QMap<int, QString> execWrapper;
};

#endif // UIFUNCTIONS_H_INCLUDED
