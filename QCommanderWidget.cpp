#include "QCommanderWidget.h"
#include "UIProxy.h"
#include "UIFunctions.h"
#include <boost/format.hpp>
#include <QHBoxLayout>

QCommanderWidget::QCommanderWidget(QWidget *parent)
    : QWidget(parent)
{
    lineEdit = new QLineEdit(this);
    lineEdit->setPlaceholderText("Input Lua code here");
    lineEdit->setFont(QFont("Courier", 12));
    comboBox = new QComboBox(this);
    comboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    layout->setMargin(0);
    setLayout(layout);
    layout->addWidget(lineEdit);
    layout->addWidget(comboBox);
    connect(lineEdit, &QLineEdit::returnPressed, this, &QCommanderWidget::onReturnPressed);
}

QCommanderWidget::~QCommanderWidget()
{
}

void QCommanderWidget::onReturnPressed()
{
    QString code = lineEdit->text();
    int type = sim_scripttype_mainscript;
    int handle = -1;
    QString name = "";
    if(comboBox->currentIndex() >= 0)
    {
        QVariantList data = comboBox->itemData(comboBox->currentIndex()).toList();
        type = data[0].toInt();
        handle = data[1].toInt();
        name = data[2].toInt();
    }
    emit execCode(code, type, name);
    lineEdit->setText("");
}

void QCommanderWidget::onScriptListChanged(QMap<int,QString> childScripts, QMap<int,QString> jointCtrlCallbacks, QMap<int,QString> customizationScripts)
{
    // save current item:
    QVariant old = comboBox->itemData(comboBox->currentIndex());

    // clear cxombo box:
    while(comboBox->count()) comboBox->removeItem(0);

    // populate combo box:
    static boost::format childScriptFmt("Child script of '%s'");
    static boost::format jointCtrlCallbackFmt("Joint Control Callback script of '%s'");
    static boost::format customizationScriptFmt("Customization script of '%s'");
    int index = 0, selectedIndex = -1;
    {
        QVariantList data;
        data << sim_scripttype_mainscript << 0 << QString();
        comboBox->addItem("Main script", data);
        if(data == old) selectedIndex = index;
        index++;
    }
    {
        QMapIterator<int,QString> i(childScripts);
        while(i.hasNext())
        {
            i.next();
            QVariantList data;
            data << sim_scripttype_childscript << i.key() << i.value();
            comboBox->addItem((childScriptFmt % i.value().toStdString()).str().c_str(), data);
            if(data == old) selectedIndex = index;
            index++;
        }
    }
    {
        QMapIterator<int,QString> i(jointCtrlCallbacks);
        while(i.hasNext())
        {
            i.next();
            QVariantList data;
            data << sim_scripttype_jointctrlcallback << i.key() << i.value();
            comboBox->addItem((jointCtrlCallbackFmt % i.value().toStdString()).str().c_str(), data);
            if(data == old) selectedIndex = index;
            index++;
        }
    }
    {
        QVariantList data;
        data << sim_scripttype_contactcallback << 0 << QString();
        comboBox->addItem("Contact callback", data);
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
            comboBox->addItem((customizationScriptFmt % i.value().toStdString()).str().c_str(), data);
            if(data == old) selectedIndex = index;
            index++;
        }
    }
    {
        QVariantList data;
        data << sim_scripttype_generalcallback << 0 << QString();
        comboBox->addItem("General callback", data);
        if(data == old) selectedIndex = index;
        index++;
    }

    if(selectedIndex >= 0)
        comboBox->setCurrentIndex(selectedIndex);
}

