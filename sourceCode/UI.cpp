#include "UI.h"

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

#include "SIM.h"
#include "stubs.h"

// UI is a singleton

UI *UI::instance = NULL;

QWidget *UI::simMainWindow = NULL;

UI::UI(QObject *parent)
    : QObject(parent)
{
}

UI::~UI()
{
    UI::instance = NULL;
}

UI * UI::getInstance(QObject *parent)
{
    if(!UI::instance)
    {
        UI::instance = new UI(parent);
        UI::simMainWindow = (QWidget *)sim::getMainWindow(1);

        uiThread(); // we remember this currentThreadId as the "UI" thread

        sim::addLog(sim_verbosity_debug, "UI(%x) constructed in thread %s", UI::instance, QThread::currentThreadId());

        SIM::getInstance()->connectSignals();
    }
    return UI::instance;
}

void UI::destroyInstance()
{
    TRACE_FUNC;

    if(UI::instance)
    {
        delete UI::instance;
        UI::instance = nullptr;

        sim::addLog(sim_verbosity_debug, "destroyed UI instance");
    }
}

void UI::setStatusBar(QPlainTextEdit *statusBar_, QSplitter *splitter_)
{
    statusBar = statusBar_;
    splitter = splitter_;
}

QList<int> UI::getStatusbarSize()
{
    return splitter->sizes();
}

void UI::setStatusbarSize(const QList<int> &sizes)
{
    splitter->setSizes(sizes);
}

void UI::setStatusbarFocus()
{
    statusBar->setFocus();
}

