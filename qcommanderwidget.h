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
#include "qcommandedit.h"

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
    QPushButton *closeButton;
    QLabel *calltipLabel;

public:
    void getSelectedScriptInfo(int &type, int &handle, QString &langSuffix);

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
    void setPreferredSandboxLang(const QString &lang);
    void setAutoAcceptCommonCompletionPrefix(bool b);
    void setShowMatchingHistory(bool b);
    void setSelectedScript(int scriptHandle, QString lang);

signals:
    void askCompletion(int scriptHandle, QString langSuffix, QString input, int pos, QStringList *clout);
    void askCallTip(int scriptHandle, QString langSuffix, QString input, int pos);
    void execCode(int scriptHandle, QString langSuffix, QString code);
    void addLog(int verbosity, QString message);

public:
    std::atomic<bool> closeFlag;

private:
    bool resizeStatusbarWhenFocused = false;
    QString preferredSandboxLang;

    QList<int> statusbarSize;
    QList<int> statusbarSizeFocused;
    bool statusbarExpanded = false;

    friend class QGlobalEventFilter;
};

#endif // QCOMMANDERWIDGET_H_INCLUDED

