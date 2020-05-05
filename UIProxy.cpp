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

QWidget *UIProxy::simMainWindow = NULL;

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
        UIProxy::simMainWindow = (QWidget *)simGetMainWindow(1);

        uiThread(); // we remember this currentThreadId as the "UI" thread

        log(sim_verbosity_debug, boost::format("UIProxy(%x) constructed in thread %s") % UIProxy::instance % QThread::currentThreadId());
    }
    return UIProxy::instance;
}

void UIProxy::destroyInstance()
{
    TRACE_FUNC;

    if(UIProxy::instance)
    {
        delete UIProxy::instance;
        UIProxy::instance = nullptr;

        log(sim_verbosity_debug, "destroyed UIProxy instance");
    }
}

void UIProxy::setStatusBar(QPlainTextEdit *statusBar_, QSplitter *splitter_)
{
    statusBar = statusBar_;
    splitter = splitter_;
}

QList<int> UIProxy::getStatusbarSize()
{
    return splitter->sizes();
}

void UIProxy::setStatusbarSize(const QList<int> &sizes)
{
    splitter->setSizes(sizes);
}

void UIProxy::setStatusbarFocus()
{
    statusBar->setFocus();
}

