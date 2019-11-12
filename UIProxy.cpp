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

        DEBUG_OUT << "UIProxy(" << UIProxy::instance << ") constructed in thread " << QThread::currentThreadId() << std::endl;
    }
    return UIProxy::instance;
}

void UIProxy::destroyInstance()
{
    DEBUG_OUT << "[enter]" << std::endl;

    if(UIProxy::instance)
    {
        delete UIProxy::instance;
        UIProxy::instance = nullptr;

        DEBUG_OUT << "destroyed UIProxy instance" << std::endl;
    }

    DEBUG_OUT << "[leave]" << std::endl;
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

