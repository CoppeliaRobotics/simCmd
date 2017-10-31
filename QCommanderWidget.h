#ifndef QCOMMANDERWIDGET_H_INCLUDED
#define QCOMMANDERWIDGET_H_INCLUDED

#include <QObject>
#include <QWidget>
#include <QLineEdit>

class QCommanderWidget : public QLineEdit
{
    Q_OBJECT

public:
    explicit QCommanderWidget(QWidget *parent = 0);
    ~QCommanderWidget();

private slots:
    void onReturnPressed();

signals:
    void execCode(QString code);
};

#endif // QCOMMANDERWIDGET_H_INCLUDED

