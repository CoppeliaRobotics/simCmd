#include "QCommanderWidget.h"
#include "UIProxy.h"

QCommanderWidget::QCommanderWidget(QWidget *parent)
    : QLineEdit(parent)
{
    connect(this, &QCommanderWidget::returnPressed, this, &QCommanderWidget::onReturnPressed);
}

QCommanderWidget::~QCommanderWidget()
{
}

void QCommanderWidget::onReturnPressed()
{
    QString code = text();
    UIProxy::getInstance()->execCode(code);
    setText("");
}

