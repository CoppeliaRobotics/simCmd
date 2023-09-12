#ifndef UIFUNCTIONS_H_INCLUDED
#define UIFUNCTIONS_H_INCLUDED

#include "config.h"

#include <QObject>
#include <QString>
#include <QMap>

#include "stubs.h"
#include "PersistentOptions.h"

class SIM : public QObject
{
    Q_OBJECT

public:
    virtual ~SIM();

    static SIM * getInstance(QObject *parent = 0);
    static void destroyInstance();

    void connectSignals();

    void setOptions(const PersistentOptions &options);

    void loadHistory();
    void appendHistory(QString code);

    void showMessage(QString s);
    void showWarning(QString s);
    void showError(QString s);

public slots:
    void clearHistory();

private:

    SIM(QObject *parent = 0);

    static SIM *instance;

    std::string getStackTopAsString(int stackHandle, const PersistentOptions &opts, int depth = 0, bool quoteStrings = true, bool insideTable = false, std::string *strType = nullptr);

    void initStringRenderingFlags();
    QStringList getMatchingStringRenderingFlags(QString shortFlag);
    void parseStringRenderingFlags(PersistentOptions *popts, const QString &code);

    QStringList getCompletion(int scriptHandle, QString word, QChar context);
    QStringList getCompletionID(int scriptHandle, QString word);
    QStringList getCompletionObjName(QString word);

    void setConvenienceVars(int scriptHandle, int stackHandle, bool check);

public slots:
    void onExecCode(QString code, int scriptHandle);
    void onAskCompletion(int scriptHandle, QString token, QChar context, QStringList *cl);
    void onAskCallTip(int scriptHandle, QString input, int pos);
    void setExecWrapper(const QString &wrapperFunc);

signals:
    void scriptListChanged(int sandboxScript, int mainScript, QMap<int,QString> childScripts, QMap<int,QString> customizationScripts, bool simRunning);
    void setCompletion(QStringList s);
    void setCallTip(QString s);
    void historyChanged(QStringList history);
    void addStatusbarMessage(const QString &s, bool html);
    void addStatusbarWarning(const QString &s, bool html);
    void addStatusbarError(const QString &s, bool html);

private:
    PersistentOptions options;
    QStringList stringRenderingFlags;
    QString execWrapper;
};

#endif // UIFUNCTIONS_H_INCLUDED

