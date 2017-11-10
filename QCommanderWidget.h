#ifndef QCOMMANDERWIDGET_H_INCLUDED
#define QCOMMANDERWIDGET_H_INCLUDED

#include <atomic>

#include <QMap>
#include <QObject>
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>

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
    void getPrevCompletion(int scriptHandleOrType, QString prefix, QString selection);
    void getNextCompletion(int scriptHandleOrType, QString prefix, QString selection);

public slots:
    void moveCursorToEnd();

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

protected:
    QCommanderEditor *editor;
    QComboBox *scriptCombo;
    QPushButton *closeButton;

    QStringList history;
    int historyIndex;

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

public slots:
    void onScriptListChanged(QMap<int,QString> childScripts, QMap<int,QString> customizationScripts, bool simRunning);

signals:
    void execCode(QString code, int scriptHandleOrType, QString scriptName);

public:
    std::atomic<bool> closeFlag;
};

#endif // QCOMMANDERWIDGET_H_INCLUDED

