#include "UIFunctions.h"
#include "debug.h"
#include "UIProxy.h"
#include "stubs.h"
#include <boost/format.hpp>

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

void UIFunctions::onExecCode(QString code, int scriptHandleOrType, QString scriptName)
{
    ASSERT_THREAD(!UI);

    QString s = code + "@" + scriptName;
    QByteArray s1 = s.toLatin1();
    simInt stackHandle = simCreateStack();
    if(stackHandle != -1)
    {
        simExecuteScriptString(scriptHandleOrType, s1.data(), stackHandle);
        simInt size = simGetStackSize(stackHandle);
        if(size == 0)
        {
            simAddStatusbarMessage("LuaCommander: there are no items in my stack");
        }
        if(size > 0)
        {
            if(size > 1)
                simAddStatusbarMessage("LuaCommander: warning: there is more than one item in my stack");
            simBool boolValue;
            simInt intValue;
            simFloat floatValue;
            simDouble doubleValue;
            simChar *stringValue;
            simInt stringSize;
            if(simGetStackBoolValue(stackHandle, &boolValue) == 1)
            {
                simAddStatusbarMessage((boost::format("LuaCommander: %s") % (boolValue ? "true" : "false")).str().c_str());
            }
            else if(simGetStackDoubleValue(stackHandle, &doubleValue) == 1)
            {
                simAddStatusbarMessage((boost::format("LuaCommander: %f") % doubleValue).str().c_str());
            }
            else if(simGetStackFloatValue(stackHandle, &floatValue) == 1)
            {
                simAddStatusbarMessage((boost::format("LuaCommander: %f") % floatValue).str().c_str());
            }
            else if(simGetStackInt32Value(stackHandle, &intValue) == 1)
            {
                simAddStatusbarMessage((boost::format("LuaCommander: %d") % intValue).str().c_str());
            }
            else if((stringValue = simGetStackStringValue(stackHandle, &stringSize)) != NULL)
            {
                simAddStatusbarMessage((boost::format("LuaCommander: %s") % stringValue).str().c_str());
                simReleaseBuffer(stringValue);
            }
        }
        simReleaseStack(stackHandle);
    }
}

