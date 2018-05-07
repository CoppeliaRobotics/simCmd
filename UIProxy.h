#ifndef UIPROXY_H_INCLUDED
#define UIPROXY_H_INCLUDED

#include "config.h"

#include <map>

#include <QObject>
#include <QString>
#include <QWidget>

#include "stubs.h"

class UIProxy : public QObject
{
    Q_OBJECT

public:
    virtual ~UIProxy();

    static UIProxy * getInstance(QObject *parent = 0);
    static void destroyInstance();

private:
    UIProxy(QObject *parent = 0);

    static UIProxy *instance;

public:
    static QWidget *vrepMainWindow;

public slots:

signals:
    void execCode(QString code, int scriptHandleOrType, QString scriptName);
    void setArrayMaxItemsDisplayed(int n);
    void setStringLongLimit(int n);
    void setMapSortKeysByName(bool b);
    void setMapSortKeysByType(bool b);
    void setMapShadowLongStrings(bool b);
    void setMapShadowBufferStrings(bool b);
    void setMapShadowSpecialStrings(bool b);
};

#endif // UIPROXY_H_INCLUDED

