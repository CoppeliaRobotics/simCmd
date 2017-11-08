#ifndef QCOMMANDERWIDGET_H_INCLUDED
#define QCOMMANDERWIDGET_H_INCLUDED

#include <QMap>
#include <QObject>
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>

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

    QStringList history;
    int historyIndex;

    void setHistoryIndex(int index);

private slots:
    void onReturnPressed();
    void onEscapePressed();
    void onUpPressed();
    void onDownPressed();

public slots:
    void onScriptListChanged(QMap<int,QString> childScripts, QMap<int,QString> jointCtrlCallbacks, QMap<int,QString> customizationScripts);

signals:
    void execCode(QString code, int scriptHandleOrType, QString scriptName);
};

#endif // QCOMMANDERWIDGET_H_INCLUDED

