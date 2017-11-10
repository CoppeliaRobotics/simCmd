#ifndef QCOMMANDERWIDGET_H_INCLUDED
#define QCOMMANDERWIDGET_H_INCLUDED

#include <atomic>

#include <QMap>
#include <QObject>
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>

class QCommanderEditor : public QLineEdit
{
    Q_OBJECT

public:
    explicit QCommanderEditor(QWidget *parent = 0);
    ~QCommanderEditor();

    void keyPressEvent(QKeyEvent *event);

signals:
    void escapePressed();
    void upPressed();
    void downPressed();

public slots:
    void moveCursorToEnd();
};

class QCommanderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QCommanderWidget(QWidget *parent = 0);
    ~QCommanderWidget();

protected:
    QCommanderEditor *editor;
    QComboBox *scriptCombo;
    QPushButton *closeButton;

    QStringList history;
    int historyIndex;

    void setHistoryIndex(int index);

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

