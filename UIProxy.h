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
    //void onSetText(TextBrowser *textbrowser, std::string text, bool suppressSignals);

signals:
    void execCode(QString code, int scriptHandleOrType, QString scriptName);
};

#endif // UIPROXY_H_INCLUDED

