#ifndef UIFUNCTIONS_H_INCLUDED
#define UIFUNCTIONS_H_INCLUDED

#include "config.h"

#include <atomic>

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

    int arrayMaxItemsDisplayed = 20;
    int stringLongLimit = 160;
    bool stringShadowBuffersInMaps = true;
    bool stringShadowSpecialsInMaps = true;
    bool truncateStringsInMaps = true;
    bool sortMapKeys = true;
    bool sortMapKeysByType = true;

private:
    UIFunctions(QObject *parent = 0);

    static UIFunctions *instance;

    std::string getStackTopAsString(int stackHandle, int depth = 0, bool quoteStrings = true, bool insideTable = false, std::string *strType = nullptr);

public slots:
    void onExecCode(QString code, int scriptHandleOrType, QString scriptName);
    void onGetPrevCompletion(int scriptHandleOrType, QString prefix, QString selection);
    void onGetNextCompletion(int scriptHandleOrType, QString prefix, QString selection);
    void onAskCallTip(int scriptHandleOrType, QString symbol);

signals:
    void scriptListChanged(QMap<int,QString> childScripts, QMap<int,QString> customizationScripts, bool simRunning);
    void setCompletion(QString s);
    void setCallTip(QString s);
};

#endif // UIFUNCTIONS_H_INCLUDED

