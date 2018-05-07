#include "UIFunctions.h"
#include "debug.h"
#include "UIProxy.h"
#include "stubs.h"
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <QRegExp>

// UIFunctions is a singleton

UIFunctions *UIFunctions::instance = NULL;

UIFunctions::UIFunctions(QObject *parent)
    : QObject(parent)
{
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

static inline bool isSpecialChar(char c)
{
    if(c >= 'a' && c <= 'z') return false;
    if(c >= 'A' && c <= 'Z') return false;
    if(c >= '0' && c <= '9') return false;
    static const char *allowed = " !@#$%^&*()_-+=[]{}\\|:;\"'<>,./?`~";
    for(int i = 0; i < strlen(allowed); i++)
        if(c == allowed[i]) return false;
    return true;
}

std::string UIFunctions::getStackTopAsString(int stackHandle, int depth, bool quoteStrings, bool insideTable, std::string *strType)
{
    simBool boolValue;
    simInt intValue;
    simFloat floatValue;
    simDouble doubleValue;
    simChar *stringValue;
    simInt stringSize;
    int n = simGetStackTableInfo(stackHandle, 0);
    if(n == sim_stack_table_map || n >= 0)
    {
        if(strType)
            *strType = "90|table";

        int oldSize = simGetStackSize(stackHandle);
        if(simUnfoldStackTable(stackHandle) != -1)
        {
            int newSize = simGetStackSize(stackHandle);
            int numItems = (newSize - oldSize + 1) / 2;

            std::stringstream ss;
            ss << "{";

            std::vector<std::vector<std::string> > lines;

            for(int i = 0; i < numItems; i++)
            {
                std::string type;

                simMoveStackItemToTop(stackHandle, oldSize - 1);
                std::string key = getStackTopAsString(stackHandle, depth + 1, false, true);

                simMoveStackItemToTop(stackHandle, oldSize - 1);
                std::string value = getStackTopAsString(stackHandle, depth + 1, true, true, &type);

                if(n > 0)
                {
                    if(i >= arrayMaxItemsDisplayed)
                    {
                        ss << " ... (" << numItems << " items)";
                        break;
                    }
                    ss << (i ? ", " : "") << value;
                }
                else
                {
                    std::vector<std::string> line;
                    line.push_back(type);
                    line.push_back(key);
                    line.push_back(value);
                    lines.push_back(line);
                }
            }

            if(lines.size())
            {
                if(mapSortKeysByName)
                {
                    std::sort(lines.begin(), lines.end(), [this](const std::vector<std::string>& a, const std::vector<std::string>& b)
                    {
                        std::string sa = mapSortKeysByType ? (a[0] + a[1]) : a[1];
                        std::string sb = mapSortKeysByType ? (b[0] + b[1]) : b[1];
                        return sa < sb;
                    });
                }

                for(int i = 0; i < lines.size(); i++)
                {
                    ss << "\n";
                    for(int d = 0; d < depth; d++)
                        ss << "    ";
                    ss << "    " << lines[i][1] << "=" << lines[i][2] << ((i + 1) < numItems ? "," : "");
                }
            }

            if(n < 0)
            {
                ss << "\n";
                for(int d = 0; d < depth; d++) ss << "    ";
            }
            ss << "}";
            return ss.str();
        }
        else
        {
            std::cout << "table unfold error. n=" << n << std::endl;
            simDebugStack(stackHandle, -1);
            return "<table:unfold-err>";
        }
    }
    else if(n == sim_stack_table_circular_ref)
    {
        if(strType)
            *strType = "90|table";

        return "<...>";
    }
    else if(simGetStackBoolValue(stackHandle, &boolValue) == 1)
    {
        if(strType)
            *strType = "10|bool";

        simPopStackItem(stackHandle, 1);
        return boolValue ? "true" : "false";
    }
    else if(simGetStackDoubleValue(stackHandle, &doubleValue) == 1)
    {
        if(strType)
            *strType = "30|number";

        simPopStackItem(stackHandle, 1);
        return boost::lexical_cast<std::string>(doubleValue);
    }
    else if(simGetStackFloatValue(stackHandle, &floatValue) == 1)
    {
        if(strType)
            *strType = "30|number";

        simPopStackItem(stackHandle, 1);
        return boost::lexical_cast<std::string>(floatValue);
    }
    else if(simGetStackInt32Value(stackHandle, &intValue) == 1)
    {
        if(strType)
            *strType = "30|number";

        simPopStackItem(stackHandle, 1);
        return boost::lexical_cast<std::string>(intValue);
    }
    else if((stringValue = simGetStackStringValue(stackHandle, &stringSize)) != NULL)
    {
        std::string s(stringValue, stringSize);

        if(strType)
        {
            if(boost::starts_with(s, "<FUNCTION"))
                *strType = "60|function";
            else if(boost::starts_with(s, "<USERDATA"))
                *strType = "70|userdata";
            else if(boost::starts_with(s, "<THREAD"))
                *strType = "80|thread";
            else
                *strType = "50|string";
        }

        simPopStackItem(stackHandle, 1);
        std::stringstream ss;

        if(quoteStrings)
            ss << "\"";

        if(insideTable)
        {
            if(mapShadowLongStrings && stringSize >= stringLongLimit)
            {
                ss << "<long string>";
            }
            else
            {
                bool isBuffer = false, isSpecial = false;
                for(int i = 0; i < std::min(stringSize, stringLongLimit); i++)
                {
                    if(mapShadowBufferStrings && stringValue[i] == 0)
                    {
                        isBuffer = true;
                        break;
                    }
                    if(mapShadowSpecialStrings && isSpecialChar(stringValue[i]))
                    {
                        isSpecial = true;
                        continue;
                    }
                }

                if(isBuffer)
                    ss << "<buffer string>";
                else if(isSpecial)
                    ss << "<string contains special chars>";
                else
                    ss << s;
            }
        }
        else ss << s;

        if(quoteStrings)
            ss << "\"";

        simReleaseBuffer(stringValue);
        return ss.str();
    }
    else
    {
        if(strType)
            *strType = "?";

        std::cout << "unable to convert stack top. n=" << n << std::endl;
        simDebugStack(stackHandle, -1);
        simPopStackItem(stackHandle, 1);
        return "?";
    }
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

void UIFunctions::onSetArrayMaxItemsDisplayed(int n)
{
    arrayMaxItemsDisplayed = n;
}

void UIFunctions::onSetStringLongLimit(int n)
{
    stringLongLimit = n;
}

void UIFunctions::onSetMapSortKeysByName(bool b)
{
    mapSortKeysByName = b;
}

void UIFunctions::onSetMapSortKeysByType(bool b)
{
    mapSortKeysByType = b;
}

void UIFunctions::onSetMapShadowLongStrings(bool b)
{
    mapShadowLongStrings = b;
}

void UIFunctions::onSetMapShadowBufferStrings(bool b)
{
    mapShadowBufferStrings = b;
}

void UIFunctions::onSetMapShadowSpecialStrings(bool b)
{
    mapShadowSpecialStrings = b;
}

