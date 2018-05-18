#ifndef QCOMMANDERWIDGET_H_INCLUDED
#define QCOMMANDERWIDGET_H_INCLUDED

#include <atomic>

#include <QMap>
#include <QObject>
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>

#include "PersistentOptions.h"

class QCommanderWidget;
class QCommanderEditor;

class QGlobalEventFilter : public QObject
{
    Q_OBJECT

private:
    QGlobalEventFilter(QCommanderWidget *commander, QCommanderEditor *widget);
    QCommanderWidget *commander_;
    QCommanderEditor *widget_;
    static QGlobalEventFilter *instance_;

public:
    bool eventFilter(QObject *object, QEvent *event);
    static void install(QCommanderWidget *commander, QCommanderEditor *widget);
    static void uninstall();
};

class QCommanderWidget;

class QCommanderEditor : public QLineEdit
{
    Q_OBJECT

public:
    explicit QCommanderEditor(QCommanderWidget *parent = 0);
    ~QCommanderEditor();

    QString tokenBehindCursor();
    void setCompletion(QString s);

    void keyPressEvent(QKeyEvent *event);

signals:
    void escapePressed();
    void upPressed();
    void downPressed();
    void getPrevCompletion(int scriptHandleOrType, QString scriptName, QString prefix, QString selection);
    void getNextCompletion(int scriptHandleOrType, QString scriptName, QString prefix, QString selection);
    void askCallTip(int scriptHandleOrType, QString symbol);
    void clear();

public slots:
    void moveCursorToEnd();
    void setCallTip(QString tip);

protected:
    bool eventFilter(QObject *obj, QEvent *event);

    QCommanderWidget *commander;
};

class QCommanderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QCommanderWidget(QWidget *parent = 0);
    ~QCommanderWidget();

    inline QCommanderEditor * editorWidget() { return editor; }

    void setOptions(const PersistentOptions &options);

protected:
    QCommanderEditor *editor;
    QComboBox *scriptCombo;
    QPushButton *closeButton;

    QStringList history;
    int historyIndex;

    QString historyPrefixFilter;

    void setHistoryIndex(int index);

public:
    bool getSelectedScriptInfo(int &type, int &handle, QString &name);
    void execute();
    void acceptCompletion();

private slots:
    void onReturnPressed();
    void onEscapePressed();
    void onUpPressed();
    void onDownPressed();
    void onClose();
    void onClear();
    void onTextEdited();
    void onFocusIn();
    void onFocusOut();
    void expandStatusbar();
    void contractStatusbar();
    void onGlobalFocusChanged(QWidget *old, QWidget *now);

public slots:
    void onScriptListChanged(QMap<int,QString> childScripts, QMap<int,QString> customizationScripts, bool simRunning);
    void setHistory(QStringList history);

signals:
    void execCode(QString code, int scriptHandleOrType, QString scriptName);

public:
    std::atomic<bool> closeFlag;

private:
    PersistentOptions options;
    QList<int> statusbarSize;
    QList<int> statusbarSizeFocused;
    bool statusbarExpanded = false;

    friend class QCommanderEditor;
    friend class QGlobalEventFilter;
};

#endif // QCOMMANDERWIDGET_H_INCLUDED

