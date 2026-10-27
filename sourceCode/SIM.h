#ifndef UIFUNCTIONS_H_INCLUDED
#define UIFUNCTIONS_H_INCLUDED

#include "config.h"
#include <QObject>
#include <QString>
#include <QMap>
#include <simPlusPlus-2/Lib.h>
#include "stubs.h"
#include "common.h"

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
    void onExecCode(sim::handle_t scriptHandle, QString code);
    void onAskCompletion(sim::handle_t scriptHandle, QString input, int pos, QStringList *clout);
    void onAskCallTip(sim::handle_t scriptHandle, QString input, int pos);
    void onCurrentScriptChanged(sim::handle_t scriptHandle);

signals:
    void setVisible(bool visible);
    void scriptListChanged(HandleStringMap sandboxScripts, HandleStringMap sandboxLang, const QString &preferredSandboxLang, sim::handle_t mainScript, HandleStringMap simulationScripts, HandleStringMap customizationScripts, HandleStringMap addons, bool simRunning, bool isRunningJustChanged);
    void setCompletion(QStringList s);
    void setCallTip(QString s);
    void historyChanged(QStringList history);
    void setAutoAcceptCommonCompletionPrefix(bool b);
    void setShowMatchingHistory(bool b);
    void setSelectedScript(sim::handle_t scriptHandle, bool silent, bool fallbackToSandbox);
    void toggleStatusbarHeight();

private:
    HandleStringMap execWrapper;
};

#endif // UIFUNCTIONS_H_INCLUDED
