#include "UIProxy.h"
#include "debug.h"

#include <QThread>
#include <QWidget>
#include <QPixmap>
#include <QImage>
#include <QLabel>
#include <QSlider>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QCheckBox>
#include <QRadioButton>
#include <QWidget>
#include <QGroupBox>
#include <QComboBox>
#include <QDialog>
#include <QTreeWidget>
#include <QTextBrowser>
#include <QScrollBar>

#include "stubs.h"

// UIProxy is a singleton

UIProxy *UIProxy::instance = NULL;

QWidget *UIProxy::vrepMainWindow = NULL;

UIProxy::UIProxy(QObject *parent)
    : QObject(parent)
{
}

UIProxy::~UIProxy()
{
    UIProxy::instance = NULL;
}

UIProxy * UIProxy::getInstance(QObject *parent)
{
    if(!UIProxy::instance)
    {
        UIProxy::instance = new UIProxy(parent);
        UIProxy::vrepMainWindow = (QWidget *)simGetMainWindow(1);

        uiThread(); // we remember this currentThreadId as the "UI" thread

        DBG << "UIProxy(" << UIProxy::instance << ") constructed in thread " << QThread::currentThreadId() << std::endl;
    }
    return UIProxy::instance;
}

void UIProxy::destroyInstance()
{
    DBG << "[enter]" << std::endl;

    if(UIProxy::instance)
    {
        delete UIProxy::instance;

        DBG << "destroyed UIProxy instance" << std::endl;
    }

    DBG << "[leave]" << std::endl;
}

void UIProxy::setStatusBar(QPlainTextEdit *statusBar_)
{
    statusBar = statusBar_;
}

void UIProxy::addStatusbarMessage(const QString &s, bool html)
{
    if(statusBar)
    {
        QScrollBar *sb = statusBar->verticalScrollBar();
        bool scrollToBottom = sb->value() == sb->maximum();

        if(html)
            statusBar->appendHtml(s);
        else
            statusBar->appendPlainText(s);

        if(scrollToBottom) sb->setValue(sb->maximum());
    }
}

void UIProxy::addStatusbarWarning(const QString &s, bool html)
{
    addStatusbarMessage(QString("<font color='#c60'>%1</font>").arg(s), true);
}

void UIProxy::addStatusbarError(const QString &s, bool html)
{
    addStatusbarMessage(QString("<font color='#c00'>%1</font>").arg(s), true);
}

