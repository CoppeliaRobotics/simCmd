#ifndef UIPROXY_H_INCLUDED
#define UIPROXY_H_INCLUDED

#include "config.h"

#include <map>

#include <QObject>
#include <QString>
#include <QWidget>
#include <QPlainTextEdit>
#include <QSplitter>

#include "stubs.h"

class UI : public QObject
{
    Q_OBJECT

public:
    virtual ~UI();

    static UI * getInstance(QObject *parent = 0);
    static void destroyInstance();

private:
    UI(QObject *parent = 0);

    static UI *instance;
    QPlainTextEdit *statusBar = nullptr;
    QSplitter *splitter = nullptr;

public:
    static QWidget *simMainWindow;

    void setStatusBar(QPlainTextEdit *statusBar, QSplitter *splitter);
    inline QPlainTextEdit *getStatusBar() {return statusBar;}
    QList<int> getStatusbarSize();
    void setStatusbarSize(const QList<int> &sizes);
    void setStatusbarFocus();

public slots:

signals:
    void execCode(QString code, int scriptHandleOrType, QString scriptName);
    void clearHistory();
};

#endif // UIPROXY_H_INCLUDED

