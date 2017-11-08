#include "UIFunctions.h"
#include "debug.h"
#include "UIProxy.h"
#include "stubs.h"
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>

// UIFunctions is a singleton

UIFunctions *UIFunctions::instance = NULL;

UIFunctions::UIFunctions(QObject *parent)
    : QObject(parent)
{
    autoReturn.store(true);
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
    else if(simGetStackInt32Value(stackHandle, &intValue) == 1)
    {
        ret = boost::lexical_cast<std::string>(intValue);
    }
    else if(simGetStackDoubleValue(stackHandle, &doubleValue) == 1)
    {
        ret = boost::lexical_cast<std::string>(doubleValue);
    }
    else if(simGetStackFloatValue(stackHandle, &floatValue) == 1)
    {
        ret = boost::lexical_cast<std::string>(floatValue);
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

    simInt ret = simExecuteScriptString(scriptHandleOrType, s1.data(), stackHandle);
    if(ret != 0)
    {
        simAddStatusbarMessage((boost::format("LuaCommander: error: %s") % getStackTopAsString(stackHandle)).str().c_str());
    }
    else
    {
        simInt size = simGetStackSize(stackHandle);
        if(size == 0)
        {
            simAddStatusbarMessage("LuaCommander: no result");
        }
        if(size > 0)
        {
            if(size > 1)
                simAddStatusbarMessage("LuaCommander: warning: more than one value returned (only showing first value)");
            simAddStatusbarMessage((boost::format("LuaCommander: result: %s") % getStackTopAsString(stackHandle)).str().c_str());
        }
    }
    simReleaseStack(stackHandle);
}

