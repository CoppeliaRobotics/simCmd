#ifndef QCOMMANDERWIDGET_H_INCLUDED
#define QCOMMANDERWIDGET_H_INCLUDED

#include <QMap>
#include <QObject>
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>

class QCommanderWidget : public QWidget
{
    Q_OBJECT

public:
    explicit QCommanderWidget(QWidget *parent = 0);
    ~QCommanderWidget();

protected:
    QLineEdit *lineEdit;
    QComboBox *comboBox;

private slots:
    void onReturnPressed();

public slots:
    void onScriptListChanged(QMap<int,QString> childScripts, QMap<int,QString> jointCtrlCallbacks, QMap<int,QString> customizationScripts);

signals:
    void execCode(QString code, int scriptHandleOrType, QString scriptName);
};

#endif // QCOMMANDERWIDGET_H_INCLUDED

