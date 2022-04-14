#ifndef QLUACOMMANDERWIDGET_H_INCLUDED
#define QLUACOMMANDERWIDGET_H_INCLUDED

#include <atomic>

#include <QMap>
#include <QObject>
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include "qcommandedit.h"
#include "PersistentOptions.h"

class QLuaCommanderWidget;
class QLuaCommanderEdit;

class QGlobalEventFilter : public QObject
{
    Q_OBJECT

private:
    QGlobalEventFilter(QLuaCommanderWidget *commander, QLuaCommanderEdit *widget);
    QLuaCommanderWidget *commander_;
    QLuaCommanderEdit *widget_;
    static QGlobalEventFilter *instance_;

public:
    bool eventFilter(QObject *object, QEvent *event);
    static void install(QLuaCommanderWidget *commander, QLuaCommanderEdit *widget);
    static void uninstall();
};

class QLuaCommanderEdit : public QCommandEdit
{
    Q_OBJECT
public:
    explicit QLuaCommanderEdit(QLuaCommanderWidget *parent = 0);
    ~QLuaCommanderEdit();

    void keyPressEvent(QKeyEvent *event);

signals:
    void clearConsole();
    void askCallTip(QString input, int pos);

public slots:

private:
    QLuaCommanderWidget *commander;
};

class QLuaCommanderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QLuaCommanderWidget(QWidget *parent = 0);
    ~QLuaCommanderWidget();

    inline QLuaCommanderEdit * editor_() {return editor;}
    void setOptions(const PersistentOptions &options);

protected:
    QLuaCommanderEdit *editor;
    QComboBox *scriptCombo;
    QPushButton *closeButton;
    QLabel *calltipLabel;

public:
    bool getSelectedScriptInfo(int &type, int &handle, QString &name);

private slots:
    void onAskCompletion(const QString &cmd, int cursorPos);
    void onAskCallTip(QString input, int pos);
    void onExecute(const QString &cmd);
    void onEscape();
    void onClose();
    void onClearConsole();
    void onFocusIn();
    void onFocusOut();
    void expandStatusbar();
    void contractStatusbar();
    void onGlobalFocusChanged(QWidget *old, QWidget *now);

public slots:
    void onSetCompletion(const QStringList &comp);
    void onSetCallTip(const QString &tip);
    void onScriptListChanged(QMap<int,QString> childScripts, QMap<int,QString> customizationScripts, bool simRunning);
    void setHistory(QStringList history);

signals:
    void askCompletion(int scriptHandleOrType, QString scriptName, QString token, QChar context);
    void askCallTip(int scriptHandleOrType, QString input, int pos);
    void execCode(QString code, int scriptHandleOrType, QString scriptName);

public:
    std::atomic<bool> closeFlag;

private:
    PersistentOptions options;
    QList<int> statusbarSize;
    QList<int> statusbarSizeFocused;
    bool statusbarExpanded = false;

    friend class QGlobalEventFilter;
};

#endif // QLUACOMMANDERWIDGET_H_INCLUDED

