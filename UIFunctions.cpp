#include "UIFunctions.h"
#include "debug.h"
#include "UIProxy.h"
#include "stubs.h"
#include <boost/lexical_cast.hpp>
#include <QRegExp>

// UIFunctions is a singleton

UIFunctions *UIFunctions::instance = NULL;

UIFunctions::UIFunctions(QObject *parent)
    : QObject(parent)
{
    //autoReturn.store(true);
    connectSignals();
}

UIFunctions::~UIFunctions()
{
    UIFunctions::instance = NULL;
}

UIFunctions * UIFunctions::getInstance(QObject *parent)
{
    if(!UIFunctions::instance)
    {
        UIFunctions::instance = new UIFunctions(parent);

        simThread(); // we remember of this currentThreadId as the "SIM" thread

        DBG << "UIFunctions(" << UIFunctions::instance << ") constructed in thread " << QThread::currentThreadId() << std::endl;
    }
    return UIFunctions::instance;
}

void UIFunctions::destroyInstance()
{
    DBG << "[enter]" << std::endl;

    if(UIFunctions::instance)
    {
        delete UIFunctions::instance;

        DBG << "destroyed UIFunctions instance" << std::endl;
    }

    DBG << "[leave]" << std::endl;
}

void UIFunctions::connectSignals()
{
    UIProxy *uiproxy = UIProxy::getInstance();
    // connect signals/slots from UIProxy to UIFunctions and vice-versa
    connect(uiproxy, &UIProxy::execCode, this, &UIFunctions::onExecCode);
}

std::string UIFunctions::getStackTopAsString(int stackHandle)
{
    simBool boolValue;
    simInt intValue;
    simFloat floatValue;
    simDouble doubleValue;
    simChar *stringValue;
    simInt stringSize;
    std::string ret = "?";
    if(simGetStackBoolValue(stackHandle, &boolValue) == 1)
    {
        ret = boolValue ? "true" : "false";
    }
    else if(simGetStackDoubleValue(stackHandle, &doubleValue) == 1)
    {
        ret = boost::lexical_cast<std::string>(doubleValue);
    }
    else if(simGetStackFloatValue(stackHandle, &floatValue) == 1)
    {
        ret = boost::lexical_cast<std::string>(floatValue);
    }
    else if(simGetStackInt32Value(stackHandle, &intValue) == 1)
    {
        ret = boost::lexical_cast<std::string>(intValue);
    }
    else if((stringValue = simGetStackStringValue(stackHandle, &stringSize)) != NULL)
    {
        ret = std::string(stringValue, stringSize);
        simReleaseBuffer(stringValue);
    }
    return ret;
}

void UIFunctions::onExecCode(QString code, int scriptHandleOrType, QString scriptName)
{
    ASSERT_THREAD(!UI);

    simInt stackHandle = simCreateStack();
    if(stackHandle == -1)
    {
        simAddStatusbarMessage("LuaCommander: error: failed to create a stack");
        return;
    }

    QString s = code;
    if(!scriptName.isEmpty()) s += "@" + scriptName;
    QByteArray s1 = s.toLatin1();

    std::string echo = "> " + code.toStdString();
    simAddStatusbarMessage(echo.c_str());

    simInt ret = simExecuteScriptString(scriptHandleOrType, s1.data(), stackHandle);
    if(ret != 0)
    {
        simAddStatusbarMessage(getStackTopAsString(stackHandle).c_str());
    }
    else
    {
        simInt size = simGetStackSize(stackHandle);
        if(size > 0)
        {
            simAddStatusbarMessage(getStackTopAsString(stackHandle).c_str());
        }
    }

    simReleaseStack(stackHandle);
}

static QStringList getCompletion(int scriptHandleOrType, QString word)
{
    simChar *buf = simGetApiFunc(scriptHandleOrType, word.toStdString().c_str());
    QString bufStr = QString::fromUtf8(buf);
    simReleaseBuffer(buf);
    return bufStr.split(QRegExp("\\s+"), QString::SkipEmptyParts);
}

static int findInsertionPoint(QStringList &l, QString w)
{
    int lo = 0, hi = l.size() - 1;
    while(lo <= hi)
    {
        int mid = (lo + hi) / 2;
        if(l[mid] < w)
            lo = mid + 1;
        else if(w < l[mid])
            hi = mid - 1;
        else
            return mid; // found at position mid
    }
    return lo; // not found, would be inserted at position lo
}

void UIFunctions::onGetPrevCompletion(int scriptHandleOrType, QString prefix, QString selection)
{
    QStringList cl = getCompletion(scriptHandleOrType, prefix);
    if(cl.isEmpty()) return;
    int i = findInsertionPoint(cl, prefix + selection) - 1;
    if(i >= 0 && i < cl.size())
        emit setCompletion(cl[i]);
}

void UIFunctions::onGetNextCompletion(int scriptHandleOrType, QString prefix, QString selection)
{
    QStringList cl = getCompletion(scriptHandleOrType, prefix);
    if(cl.isEmpty()) return;
    int i = selection == "" ? 0 : (findInsertionPoint(cl, prefix + selection) + 1);
    if(i >= 0 && i < cl.size())
        emit setCompletion(cl[i]);
}

void UIFunctions::onAskCallTip(int scriptHandleOrType, QString symbol)
{
    simChar *buf = simGetApiInfo(scriptHandleOrType, symbol.toStdString().c_str());
    QString bufStr = QString::fromUtf8(buf);
    if(!bufStr.isEmpty())
        emit setCallTip(bufStr);
    simReleaseBuffer(buf);
}

