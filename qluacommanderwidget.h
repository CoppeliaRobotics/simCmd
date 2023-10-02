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

protected:
    QLuaCommanderEdit *editor;
    QComboBox *scriptCombo;
    QPushButton *closeButton;
    QLabel *calltipLabel;

public:
    void getSelectedScriptInfo(int &handle, QString &langSuffix);

private slots:
    void onAskCompletion(const QString &cmd, int cursorPos);
    void onAskCallTip(QString input, int pos);
    void onExecute(const QString &cmd);
    void onEscape();
    void onEditorCleared();
    void onEditorChanged(QString text);
    void onEditorCursorChanged(int oldPos, int newPos);
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
    void onScriptListChanged(int sandboxScript, int mainScript, QMap<int,QString> childScripts, QMap<int,QString> customizationScripts, bool simRunning);
    void setHistory(QStringList history);
    void setResizeStatusbarWhenFocused(bool b);
    void setAutoAcceptCommonCompletionPrefix(bool b);
    void setShowMatchingHistory(bool b);

signals:
    void askCompletion(int scriptHandle, QString langSuffix, QString input, int pos, QStringList *clout);
    void askCallTip(int scriptHandle, QString langSuffix, QString input, int pos);
    void execCode(int scriptHandle, QString langSuffix, QString code);

public:
    std::atomic<bool> closeFlag;

private:
    bool resizeStatusbarWhenFocused = false;
    QList<int> statusbarSize;
    QList<int> statusbarSizeFocused;
    bool statusbarExpanded = false;

    friend class QGlobalEventFilter;
};

#endif // QLUACOMMANDERWIDGET_H_INCLUDED

