#include "QCommanderWidget.h"
#include "UIProxy.h"
#include "UIFunctions.h"
#include <boost/format.hpp>
#include <QHBoxLayout>
#include <QKeyEvent>

QCommanderEditor::QCommanderEditor(QWidget *parent)
    : QLineEdit(parent)
{
}

QCommanderEditor::~QCommanderEditor()
{
}

void QCommanderEditor::keyPressEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Escape)
    {
        emit escapePressed();
    }
    else if(event->key() == Qt::Key_Up)
    {
        emit upPressed();
    }
    else if(event->key() == Qt::Key_Down)
    {
        emit downPressed();
    }

    QLineEdit::keyPressEvent(event);
}

QCommanderWidget::QCommanderWidget(QWidget *parent)
    : QWidget(parent),
      historyIndex(-1)
{
    editor = new QCommanderEditor(this);
    editor->setPlaceholderText("Input Lua code here");
    editor->setFont(QFont("Courier", 12));
    scriptCombo = new QComboBox(this);
    scriptCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    layout->setMargin(0);
    setLayout(layout);
    layout->addWidget(editor);
    layout->addWidget(scriptCombo);
    connect(editor, &QCommanderEditor::returnPressed, this, &QCommanderWidget::onReturnPressed);
    connect(editor, &QCommanderEditor::escapePressed, this, &QCommanderWidget::onEscapePressed);
    connect(editor, &QCommanderEditor::upPressed, this, &QCommanderWidget::onUpPressed);
    connect(editor, &QCommanderEditor::downPressed, this, &QCommanderWidget::onDownPressed);
}

QCommanderWidget::~QCommanderWidget()
{
}

void QCommanderWidget::setHistoryIndex(int index)
{
    historyIndex = index;

    if(index == -1)
    {
        editor->setText("");
        return;
    }

    if(historyIndex < 0)
        historyIndex = 0;

    if(historyIndex >= history.size() - 1)
        historyIndex = history.size() - 1;

    editor->setText(history[historyIndex]);
    editor->setCursorPosition(history[historyIndex].size() - 1);
}

void QCommanderWidget::onReturnPressed()
{
    QString code = editor->text();
    int type = sim_scripttype_mainscript;
    int handle = -1;
    QString name = "";
    if(scriptCombo->currentIndex() >= 0)
    {
        QVariantList data = scriptCombo->itemData(scriptCombo->currentIndex()).toList();
        type = data[0].toInt();
        handle = data[1].toInt();
        name = data[2].toString();
    }
    emit execCode(code, type, name);
    history << code;
    setHistoryIndex(-1);
}

void QCommanderWidget::onEscapePressed()
{
    setHistoryIndex(-1);
}

void QCommanderWidget::onUpPressed()
{
    if(historyIndex == -1) setHistoryIndex(history.size() - 1);
    else if(historyIndex > 0) setHistoryIndex(--historyIndex);
}

void QCommanderWidget::onDownPressed()
{
    if(historyIndex >= 0) setHistoryIndex(++historyIndex);
}

void QCommanderWidget::onScriptListChanged(QMap<int,QString> childScripts, QMap<int,QString> jointCtrlCallbacks, QMap<int,QString> customizationScripts, bool simRunning)
{
    // save current item:
    QVariant old = scriptCombo->itemData(scriptCombo->currentIndex());

    // clear cxombo box:
    while(scriptCombo->count()) scriptCombo->removeItem(0);

    // populate combo box:
    static boost::format childScriptFmt("Child script of '%s'");
    static boost::format jointCtrlCallbackFmt("Joint Control Callback script of '%s'");
    static boost::format customizationScriptFmt("Customization script of '%s'");
    int index = 0, selectedIndex = -1;
    {
        QVariantList data;
        data << sim_scripttype_sandboxscript << 0 << QString();
        scriptCombo->addItem("Sandbox script", data);
        if(data == old) selectedIndex = index;
        index++;
    }
    if(simRunning) {
        QVariantList data;
        data << sim_scripttype_mainscript << 0 << QString();
        scriptCombo->addItem("Main script", data);
        if(data == old) selectedIndex = index;
        index++;
    }
    if(simRunning) {
        QMapIterator<int,QString> i(childScripts);
        while(i.hasNext())
        {
            i.next();
            QVariantList data;
            data << sim_scripttype_childscript << i.key() << i.value();
            scriptCombo->addItem((childScriptFmt % i.value().toStdString()).str().c_str(), data);
            if(data == old) selectedIndex = index;
            index++;
        }
    }
    if(simRunning) {
        QMapIterator<int,QString> i(jointCtrlCallbacks);
        while(i.hasNext())
        {
            i.next();
            QVariantList data;
            data << sim_scripttype_jointctrlcallback << i.key() << i.value();
            scriptCombo->addItem((jointCtrlCallbackFmt % i.value().toStdString()).str().c_str(), data);
            if(data == old) selectedIndex = index;
            index++;
        }
    }
    if(simRunning) {
        QVariantList data;
        data << sim_scripttype_contactcallback << 0 << QString();
        scriptCombo->addItem("Contact callback", data);
        if(data == old) selectedIndex = index;
        index++;
    }
    {
        QMapIterator<int,QString> i(customizationScripts);
        while(i.hasNext())
        {
            i.next();
            QVariantList data;
            data << sim_scripttype_customizationscript << i.key() << i.value();
            scriptCombo->addItem((customizationScriptFmt % i.value().toStdString()).str().c_str(), data);
            if(data == old) selectedIndex = index;
            index++;
        }
    }
    {
        QVariantList data;
        data << sim_scripttype_generalcallback << 0 << QString();
        scriptCombo->addItem("General callback", data);
        if(data == old) selectedIndex = index;
        index++;
    }

    if(selectedIndex >= 0)
        scriptCombo->setCurrentIndex(selectedIndex);
    else
        scriptCombo->setCurrentIndex(0);
}

