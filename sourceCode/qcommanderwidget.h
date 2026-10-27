#ifndef QCOMMANDERWIDGET_H_INCLUDED
#define QCOMMANDERWIDGET_H_INCLUDED

#include <atomic>

#include <QMap>
#include <QObject>
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <simPlusPlus-2/Lib.h>
#include "qcommandedit.h"
#include "common.h"

class QCommanderWidget;
class QCommanderEdit;

class QGlobalEventFilter : public QObject
{
    Q_OBJECT

private:
    QGlobalEventFilter(QCommanderWidget *commander, QCommanderEdit *widget);
    QCommanderWidget *commander_;
    QCommanderEdit *widget_;
    static QGlobalEventFilter *instance_;

public:
    bool eventFilter(QObject *object, QEvent *event);
    static void install(QCommanderWidget *commander, QCommanderEdit *widget);
    static void uninstall();
};

class QCommanderEdit : public QCommandEdit
{
    Q_OBJECT
public:
    explicit QCommanderEdit(QCommanderWidget *parent = 0);
    ~QCommanderEdit();

    void keyPressEvent(QKeyEvent *event);

signals:
    void clearConsole();
    void askCallTip(QString input, int pos);

public slots:

private:
    QCommanderWidget *commander;
};

class QCommanderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QCommanderWidget(QWidget *parent = 0);
    ~QCommanderWidget();

    inline QCommanderEdit * editor_() {return editor;}

protected:
    QCommanderEdit *editor;
    QComboBox *scriptCombo;
    QLabel *calltipLabel;

public:
    sim::handle_t getSelectedScriptHandle();
    bool statusbarExpanded();

private slots:
    void onAskCompletion(const QString &cmd, int cursorPos);
    void onAskCallTip(QString input, int pos);
    void onExecute(const QString &cmd);
    void onEscape();
    void onEditorCleared();
    void onEditorChanged(QString text);
    void onEditorCursorChanged(int oldPos, int newPos);
    void onClearConsole();
    void expandStatusbar();
    void contractStatusbar();
    void toggleStatusbarHeight();

public slots:
    void onSetCompletion(const QStringList &comp);
    void onSetCallTip(const QString &tip);
    void onScriptListChanged(HandleStringMap sandboxScripts, HandleStringMap sandboxLang, const QString &preferredSandboxLang, sim::handle_t mainScript, HandleStringMap simulationScripts, HandleStringMap customizationScripts, HandleStringMap addons, bool simRunning, bool isRunningJustChanged);
    void setHistory(QStringList history);
    void setAutoAcceptCommonCompletionPrefix(bool b);
    void setShowMatchingHistory(bool b);
    void setSelectedScript(sim::handle_t scriptHandle, bool silent, bool fallbackToSandbox);
    void selectPreviousOrNextScript(int delta);

signals:
    void askCompletion(sim::handle_t scriptHandle, QString input, int pos, QStringList *clout);
    void askCallTip(sim::handle_t scriptHandle, QString input, int pos);
    void execCode(sim::handle_t scriptHandle, QString code);
    void addLog(int verbosity, QString message);
    void currentScriptChanged(sim::handle_t scriptHandle);

private:
    HandleStringMap sandboxScripts;
    QList<int> statusbarSize;
    QList<int> statusbarSizeFocused;

    friend class QGlobalEventFilter;
};

#endif // QCOMMANDERWIDGET_H_INCLUDED
