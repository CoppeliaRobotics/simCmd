#ifndef UIPROXY_H_INCLUDED
#define UIPROXY_H_INCLUDED

#include "config.h"

#include <map>

#include <QObject>
#include <QString>
#include <QWidget>
#include <QPlainTextEdit>

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
    QPlainTextEdit *statusBar = nullptr;

public:
    static QWidget *vrepMainWindow;

    void setStatusBar(QPlainTextEdit *statusBar);

public slots:

signals:
    void execCode(QString code, int scriptHandleOrType, QString scriptName);
    void clearHistory();
};

#endif // UIPROXY_H_INCLUDED

