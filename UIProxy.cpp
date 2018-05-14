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

