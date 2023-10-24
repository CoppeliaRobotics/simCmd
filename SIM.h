#ifndef UIFUNCTIONS_H_INCLUDED
#define UIFUNCTIONS_H_INCLUDED

#include "config.h"

#include <QObject>
#include <QString>
#include <QMap>

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
    void onExecCode(int scriptHandle, QString lang, QString code);
    void onAskCompletion(int scriptHandle, QString lang, QString input, int pos, QStringList *clout);
    void onAskCallTip(int scriptHandle, QString lang, QString input, int pos);

signals:
    void setVisible(bool visible);
    void scriptListChanged(int sandboxScript, int mainScript, QMap<int,QString> childScripts, QMap<int,QString> customizationScripts, bool simRunning, bool isRunningJustChanged, bool havePython);
    void setCompletion(QStringList s);
    void setCallTip(QString s);
    void historyChanged(QStringList history);
    void setResizeStatusbarWhenFocused(bool b);
    void setPreferredSandboxLang(QString lang);
    void setAutoAcceptCommonCompletionPrefix(bool b);
    void setShowMatchingHistory(bool b);
    void setSelectedScript(int scriptHandle, QString lang, bool silent, bool fallbackToSandbox);

private:
    QMap<int, QString> execWrapper;
};

#endif // UIFUNCTIONS_H_INCLUDED

