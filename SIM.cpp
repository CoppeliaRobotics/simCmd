#include "SIM.h"
#include "UI.h"
#include "stubs.h"
#include <simPlusPlus/Lib.h>
#include <simStack/stackObject.h>
#include <simStack/stackNull.h>
#include <simStack/stackBool.h>
#include <simStack/stackNumber.h>
#include <simStack/stackString.h>
#include <simStack/stackArray.h>
#include <simStack/stackMap.h>
#include <stdexcept>
#include <iomanip>
#include <iostream>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <QRegExp>

// SIM is a singleton

SIM *SIM::instance = NULL;

SIM::SIM(QObject *parent)
    : QObject(parent)
{
    initStringRenderingFlags();
}

SIM::~SIM()
{
    SIM::instance = NULL;
}

SIM * SIM::getInstance(QObject *parent)
{
    if(!SIM::instance)
    {
        SIM::instance = new SIM(parent);

        simThread(); // we remember of this currentThreadId as the "SIM" thread

        sim::addLog(sim_verbosity_debug, "SIM(%x) constructed in thread %s", SIM::instance, QThread::currentThreadId());
    }
    return SIM::instance;
}

void SIM::destroyInstance()
{
    TRACE_FUNC;

    if(SIM::instance)
    {
        delete SIM::instance;
        SIM::instance = nullptr;

        sim::addLog(sim_verbosity_debug, "destroyed SIM instance");
    }
}

void SIM::connectSignals()
{
    UI *ui = UI::getInstance();
    SIM *sim = this;
    // connect signals/slots from UI to SIM and vice-versa
    connect(ui, &UI::execCode, sim, &SIM::onExecCode);
    connect(ui, &UI::clearHistory, sim, &SIM::clearHistory);
}

QStringList loadHistoryData()
{
    QStringList hist;
    try
    {
        std::string pdata = sim::persistentDataRead("LuaCommander.history");
        QString s = QString::fromStdString(pdata);
        hist = s.split(QRegExp("(\\r\\n)|(\\n\\r)|\\r|\\n"),
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
            QString::SkipEmptyParts
#else
            Qt::SkipEmptyParts
#endif
        );
    }
    catch(sim::api_error &ex)
    {
        sim::addLog(sim_verbosity_debug, "failed to read history persistent block");
    }
    return hist;
}

void saveHistoryData(QStringList hist)
{
    QString histStr = hist.join("\n");
    try
    {
        sim::persistentDataWrite("LuaCommander.history", histStr.toStdString(), 1);
    }
    catch(sim::api_error &ex)
    {
        sim::addLog(sim_verbosity_debug, "failed to write history persistent block");
    }
}

void SIM::loadHistory()
{
    QStringList hist = loadHistoryData();
    emit historyChanged(hist);
}

void SIM::clearHistory()
{
    QStringList hist;
    saveHistoryData(hist);
    emit historyChanged(hist);
}

inline void reverse(QStringList &lst)
{
    for(size_t i = 0; i < lst.size() / 2; ++i)
        std::swap(lst[i], lst[lst.size() - i - 1]);
}

void SIM::appendHistory(QString cmd)
{
    QStringList hist = loadHistoryData();

    if(!options.historySkipRepeated || hist.isEmpty() || hist[hist.size() - 1] != cmd)
        hist << cmd;

    if(options.historyRemoveDups)
    {
        reverse(hist);
        hist.removeDuplicates();
        reverse(hist);
    }

    if(options.historySize >= 0)
    {
        int numToRemove = hist.size() - options.historySize;
        if(numToRemove > 0)
            hist.erase(hist.begin(), hist.begin() + numToRemove);
    }

    saveHistoryData(hist);

    emit historyChanged(hist);
}

void SIM::setOptions(const PersistentOptions &options)
{
    this->options = options;
}

static inline bool isSpecialChar(char c)
{
    if(c == '\n' || c == '\r' || c == '\t') return false;
    return c < 32 || c > 126;
}

std::string escapeSpecialChars(std::string s)
{
    std::stringstream ss;
    for(size_t i = 0; i < s.length(); i++)
    {
        char c = s[i];
        if(c == '\0')
            ss << "\\0";
        else if(c == '\n')
            ss << "\\n";
        else if(c == '\r')
            ss << "\\r";
        else if(c == '\t')
            ss << "\\t";
        else if(c == '\\')
            ss << "\\\\";
        else if(isSpecialChar(c))
            ss << '?';
        else
            ss << c;
    }
    return ss.str();
}

std::string SIM::getStackTopAsString(int stackHandle, const PersistentOptions &opts, int depth, bool quoteStrings, bool insideTable, std::string *strType)
{
    int itemType = sim::getStackItemType(stackHandle, -1);
    bool boolValue;
    double doubleValue;
    int intValue;
    long long int int64Value;
    char *stringValue;
    int stringSize;
    int n = sim::getStackTableInfo(stackHandle, 0);

    if(itemType == sim_stackitem_null)
    {
        if(strType)
            *strType = "99|nil";

        return "nil";
    }
    else if(itemType == sim_stackitem_table && (n == sim_stack_table_map || n >= 0))
    {
        if(strType)
            *strType = "90|table";

        int oldSize = sim::getStackSize(stackHandle);

        sim::unfoldStackTable(stackHandle);

        if(opts.mapMaxDepth >= 0 && depth > opts.mapMaxDepth)
            return "<...>";

        int newSize = sim::getStackSize(stackHandle);
        int numItems = (newSize - oldSize + 1) / 2;

        std::stringstream ss;
        ss << "{";

        std::vector<std::vector<std::string> > lines;

        for(int i = 0; i < numItems; i++)
        {
            std::string type;

            sim::moveStackItemToTop(stackHandle, oldSize - 1);
            std::string key = getStackTopAsString(stackHandle, opts, depth + 1, false, true, &type);

            // fix for rendering of numeric string keys:
            if(type.substr(3) == "string")
            {
                try
                {
                    boost::lexical_cast<int>(key);
                    key = (boost::format("\"%s\"") % key).str();
                }
                catch(std::exception &ex) {}
            }

            sim::moveStackItemToTop(stackHandle, oldSize - 1);
            std::string value = getStackTopAsString(stackHandle, opts, depth + 1, true, true, &type);

            if(n > 0)
            {
                if(opts.arrayMaxItemsDisplayed >= 0 && i >= opts.arrayMaxItemsDisplayed)
                {
                    ss << (i ? " " : "") << "... (" << numItems << " items)";
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
            if(opts.mapSortKeysByName || opts.mapSortKeysByType)
            {
                std::sort(lines.begin(), lines.end(), [opts](const std::vector<std::string>& a, const std::vector<std::string>& b)
                {
                    std::string sa, sb;
                    if(opts.mapSortKeysByType)
                    {
                        sa += a[0];
                        sb += b[0];
                    }
                    if(opts.mapSortKeysByName)
                    {
                        sa += a[1];
                        sb += b[1];
                    }
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
    else if(itemType == sim_stackitem_table && n == sim_stack_table_circular_ref)
    {
        if(strType)
            *strType = "90|table";

        sim::popStackItem(stackHandle, 1);
        return "<...>";
    }
    else if(itemType == sim_stackitem_bool && sim::getStackBoolValue(stackHandle, &boolValue) == 1)
    {
        if(strType)
            *strType = "10|bool";

        sim::popStackItem(stackHandle, 1);
        return boolValue ? "true" : "false";
    }
    else if(itemType == sim_stackitem_integer && sim::getStackInt64Value(stackHandle, &int64Value) == 1)
    {
        if(strType)
            *strType = "30|number";

        sim::popStackItem(stackHandle, 1);
        return std::to_string(int64Value);
    }
    else if(itemType == sim_stackitem_integer && sim::getStackInt32Value(stackHandle, &intValue) == 1)
    {
        if(strType)
            *strType = "30|number";

        sim::popStackItem(stackHandle, 1);
        return std::to_string(intValue);
    }
    else if(itemType == sim_stackitem_double && sim::getStackDoubleValue(stackHandle, &doubleValue) == 1)
    {
        if(strType)
            *strType = "30|number";

        sim::popStackItem(stackHandle, 1);
        std::ostringstream ss;
        ss << std::setprecision(opts.floatPrecision) << doubleValue;
        auto ret = ss.str();
        if(ret.find(".") == std::string::npos)
            ret += ".0";
        return ret;
    }
    else if(itemType == sim_stackitem_string && (stringValue = sim::getStackStringValue(stackHandle, &stringSize)) != NULL)
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

        sim::popStackItem(stackHandle, 1);

        if(insideTable)
        {
            bool isBuffer = false, isSpecial = false, isLong = false;
            for(size_t i = 0; i < s.length(); i++)
            {
                if(opts.mapShadowLongStrings && i >= opts.stringLongLimit && opts.stringLongLimit >= 0)
                    isLong = true;
                if(opts.mapShadowBufferStrings && s[i] == 0)
                    isBuffer = true;
                if(opts.mapShadowSpecialStrings && isSpecialChar(s[i]))
                    isSpecial = true;
                if(isLong || isBuffer) break;
            }
            if(isBuffer) s = "<buffer string>";
            else if(isLong) s = "<long string>";
            else if(isSpecial) s = "<string contains special chars>";
        }

        if(opts.stringEscapeSpecials || insideTable)
            s = escapeSpecialChars(s);

        if(quoteStrings)
        {
            boost::replace_all(s, "\"", "\\\"");
            s = "\"" + s + "\"";
        }

        sim::releaseBuffer(stringValue);

        return s;
    }
    else if(itemType == sim_stackitem_func)
    {
        if(strType)
            *strType = "60|function";

        sim::popStackItem(stackHandle, 1);

        return "<<func>>";
    }
    else if(itemType == sim_stackitem_userdat)
    {
        if(strType)
            *strType = "70|userdata";

        sim::popStackItem(stackHandle, 1);

        return "<<userdat>>";
    }
    else if(itemType == sim_stackitem_thread)
    {
        if(strType)
            *strType = "80|thread";

        sim::popStackItem(stackHandle, 1);

        return "<<thread>>";
    }
    else if(itemType == sim_stackitem_lightuserdat)
    {
        if(strType)
            *strType = "75|lightuserdata";

        sim::popStackItem(stackHandle, 1);

        return "<<lightuserdat>>";
    }
    else
    {
        if(strType)
            *strType = "?";

        sim::addLog(sim_verbosity_errors, "unable to convert stack top: itemType=%d, n=%d", itemType, n);
        sim::debugStack(stackHandle, -1);
        sim::popStackItem(stackHandle, 1);
        return "?";
    }
}

void SIM::initStringRenderingFlags()
{
    stringRenderingFlags.clear();
    stringRenderingFlags << "retvals";
    stringRenderingFlags << "sort";
    stringRenderingFlags << "precision";
    stringRenderingFlags << "depth";
    stringRenderingFlags << "escape";
}

QStringList SIM::getMatchingStringRenderingFlags(QString shortFlag)
{
    QStringList ret;
    foreach(const QString &flag, stringRenderingFlags)
    {
        if(flag.startsWith(shortFlag))
            ret << flag;
    }
    return ret;
}

void SIM::parseStringRenderingFlags(PersistentOptions *popts, const QString &code)
{
    int index = code.lastIndexOf("--");
    if(index == -1) return;
    QVector<QStringRef> flagList = code.midRef(index + 2).split(",");
    foreach(const QStringRef &s, flagList)
    {
        QString t = s.trimmed().toString();
        QString optName = t.section('=', 0, 0);
        QString optVal = t.section('=', 1);

        QStringList matchingOptions = getMatchingStringRenderingFlags(optName);
        if(matchingOptions.size() > 1)
            throw std::runtime_error((boost::format("string rendering option '%s' is ambiguous (would match: %s)") % optName.toStdString() % matchingOptions.join(", ").toStdString()).str());
        if(matchingOptions.size() == 0)
            throw std::runtime_error((boost::format("unrecognized string rendering option: '%s' (valid options are: %s)") % optName.toStdString() % stringRenderingFlags.join(", ").toStdString()).str());
        optName = matchingOptions[0];

        if(optName == "retvals")
        {
            if(optVal != "1" && optVal != "*")
                throw std::runtime_error((boost::format("invalid 'retvals' option: '%s' (valid values are: 1, *)") % optVal.toStdString()).str());
            popts->printAllReturnedValues = optVal == "*";
        }
        if(optName == "sort")
        {
            if(optVal != "tk" && optVal != "t" && optVal != "k" && optVal != "off")
                throw std::runtime_error((boost::format("invalid 'sort' option: '%s' (valid values are: k, t, tk, off)") % optVal.toStdString()).str());
            popts->mapSortKeysByType = optVal == "tk" || optVal == "t";
            popts->mapSortKeysByName = optVal == "tk" || optVal == "k";
        }
        if(optName == "precision")
        {
            int p = boost::lexical_cast<int>(optVal.toStdString());
            if(p < 0 || p > 400)
                throw std::runtime_error((boost::format("invalid 'precision' option: '%d' (should be between 0 and 400)") % optVal.toStdString()).str());
            popts->floatPrecision = p;
        }
        if(optName == "depth")
        {
            int d = boost::lexical_cast<int>(optVal.toStdString());
            popts->mapMaxDepth = d;
        }
        if(optName == "escape")
        {
            int d = boost::lexical_cast<int>(optVal.toStdString());
            popts->stringEscapeSpecials = d;
        }
    }
}

void SIM::showMessage(QString s)
{
    if(sim::getBoolParam(sim_boolparam_headless))
        std::cout << s.toStdString() << std::endl;
    else
        sim::addLog(sim_verbosity_msgs|sim_verbosity_undecorated, s.toStdString());
}

void SIM::showWarning(QString s)
{
    if(sim::getBoolParam(sim_boolparam_headless))
        std::cout << s.toStdString() << std::endl;
    else
        sim::addLog(sim_verbosity_scriptwarnings|sim_verbosity_undecorated, s.toStdString());
}

void SIM::showError(QString s)
{
    if(sim::getBoolParam(sim_boolparam_headless))
        std::cout << s.toStdString() << std::endl;
    else
        sim::addLog(sim_verbosity_scripterrors|sim_verbosity_undecorated, s.toStdString());
}

void SIM::onAskCompletion(int scriptHandle, QString token, QChar context, QStringList *clout)
{
    ASSERT_THREAD(!UI);

    QStringList cl = getCompletion(scriptHandle, token, context);
    if(clout) *clout = cl;

    // the QCommandEdit widget wants completions without the initial token part
    QStringList cl2;
    for(const QString &s : cl)
    {
        if(s.startsWith(token))
            cl2 << s.mid(token.length());
    }
    emit setCompletion(cl2);
}

void SIM::setConvenienceVars(int scriptHandle, int stackHandle, bool check)
{
    if(check)
    {
        try
        {
            sim::executeScriptString(scriptHandle, "H==sim.getObject", stackHandle);
            bool boolValue;
            if(sim::getStackBoolValue(stackHandle, &boolValue) == 1)
            {
                sim::popStackItem(stackHandle, 1);
                if(!boolValue)
                    showWarning("cannot change 'H' variable");
            }
            else sim::addLog(sim_verbosity_debug, "non-bool on stack");
        }
        catch(sim::api_error &ex)
        {
            sim::addLog(sim_verbosity_debug, ex.what());
        }
    }

    try
    {
        sim::executeScriptString(scriptHandle, "H=sim.getObject", stackHandle);
        sim::executeScriptString(scriptHandle, "SEL=sim.getObjectSel()", stackHandle);
        sim::executeScriptString(scriptHandle, "SEL1=SEL[#SEL]", stackHandle);
    }
    catch(sim::api_error &ex)
    {
        sim::addLog(sim_verbosity_debug, ex.what());
    }
}

void SIM::onExecCode(QString code, int scriptHandle)
{
    ASSERT_THREAD(!UI);

    appendHistory(code);

    int stackHandle = sim::createStack();

    if(!sim::getBoolParam(sim_boolparam_headless))
        showMessage("> " + code);

    PersistentOptions opts = options;
    try
    {
        parseStringRenderingFlags(&opts, code);
    }
    catch(std::exception &ex)
    {
#ifdef LOCAL_RESULT_RENDERING
        QString m(ex.what());
        showWarning(m);
#endif // LOCAL_RESULT_RENDERING
    }

    if(opts.setConvenienceVars)
        setConvenienceVars(scriptHandle, stackHandle, false);
    bool err = false;
    try
    {
        auto i = execWrapper.find(scriptHandle);
        if(i != execWrapper.end())
        {
            sim::pushStringOntoStack(stackHandle, code.toStdString());
            sim::callScriptFunctionEx(scriptHandle, i.value().toStdString(), stackHandle);
        }
        else
        {
            sim::executeScriptString(scriptHandle, code.toStdString(), stackHandle);
        }
    }
    catch(sim::api_error &ex)
    {
#ifdef LOCAL_RESULT_RENDERING
        PersistentOptions optsE(opts);
        optsE.stringEscapeSpecials = false;
        std::string s = getStackTopAsString(stackHandle, optsE, 0, false);
        QString m = QString::fromStdString(s);
        showError(m);
        err = true;
#endif // LOCAL_RESULT_RENDERING
    }

    if(!err)
    {
#ifdef LOCAL_RESULT_RENDERING
        int size = sim::getStackSize(stackHandle);
        if(!opts.printAllReturnedValues && size > 1)
        {
            if(opts.warnAboutMultipleReturnedValues)
                showWarning("more than one return value");
            size = 1;
        }

        QString result;
        for(int i = 0; i < size; i++)
        {
            sim::moveStackItemToTop(stackHandle, 0);
            std::string stackTopStr = getStackTopAsString(stackHandle, opts);
            if(i) result.append(", ");
            result.append(QString::fromStdString(stackTopStr));
        }
#endif // LOCAL_RESULT_RENDERING

        if(opts.setConvenienceVars)
            setConvenienceVars(scriptHandle, stackHandle, true);

#ifdef LOCAL_RESULT_RENDERING
        if(size > 0)
            showMessage(result);
#endif // LOCAL_RESULT_RENDERING
    }

    sim::releaseStack(stackHandle);
    try
    {
        sim::announceSceneContentChange();
    }
    catch(sim::api_error &ex)
    {
        sim::addLog(sim_verbosity_errors, ex.what());
    }
}

QStringList SIM::getCompletion(int scriptHandle, QString word, QChar context)
{
    QStringList result;

    if(context == 'i') result = getCompletionID(scriptHandle, word);
    else if(context == 'H') result = getCompletionObjName(word);

    result.sort();

    return result;
}

QStringList SIM::getCompletionID(int scriptHandle, QString word)
{
    ASSERT_THREAD(!UI);

    QStringList result;

    if(options.dynamicCompletion)
    {
        int stackHandle = sim::createStack();
        if(stackHandle == -1) return {};

        int dotIdx = word.lastIndexOf('.');
        bool global = dotIdx == -1;
        QString parent = global ? "_G" : word.left(dotIdx);
        QString child = global ? word : word.mid(dotIdx + 1);

        try
        {
            sim::executeScriptString(scriptHandle, parent.toStdString(), stackHandle);
        }
        catch(sim::api_error &ex)
        {
            return {};
        }
        if(sim::getStackSize(stackHandle) > 0)
        {
            int n = sim::getStackTableInfo(stackHandle, 0);
            if(n == sim_stack_table_map)
            {
                int oldSize = sim::getStackSize(stackHandle);
                sim::unfoldStackTable(stackHandle);
                int newSize = sim::getStackSize(stackHandle);
                int numItems = (newSize - oldSize + 1) / 2;
                for(int i = 0; i < numItems; i++)
                {
                    sim::moveStackItemToTop(stackHandle, oldSize - 1);
                    QString key = QString::fromStdString(getStackTopAsString(stackHandle, options, 0, false));

                    sim::moveStackItemToTop(stackHandle, oldSize - 1);
                    std::string value = getStackTopAsString(stackHandle, options, 0, false);

                    if(key.startsWith(child))
                    {
                        if(key.startsWith("_") && child.isEmpty())
                            continue;
                        if(!global)
                            key = parent + "." + key;
                        result << key;
                    }
                }
            }
        }
    }
    else
    {
        std::vector<std::string> funcs = sim::getApiFunc(scriptHandle, word.toStdString());
        for(const auto &s : funcs)
            result << QString::fromStdString(s);
    }

#ifndef NDEBUG
    sim::addLog(sim_verbosity_debug, "dynamic=%d, prefix=%s: ", options.dynamicCompletion, word.toStdString());
    for(int i = 0; i < result.size(); ++i)
        sim::addLog(sim_verbosity_debug, result[i].toStdString());
#endif

    return result;
}

QStringList SIM::getCompletionObjName(QString word)
{
    ASSERT_THREAD(!UI);

    QStringList result;
    for(int handle : sim::getObjects(sim_handle_all))
    {
        QString nameStr(QString::fromStdString(sim::getObjectAlias(handle, 5)));
        if(nameStr.startsWith(word))
            result << nameStr;
    }

    return result;
}

static inline bool isID(QChar c)
{
    return c.isLetterOrNumber() || c == '_' || c == '.';
}

void SIM::onAskCallTip(int scriptHandle, QString input, int pos)
{
    auto getApiInfo = [=](const int &scriptHandle, const QString &symbol) -> QString
    {
        if(symbol == "") return "";
        QString tip{QString::fromStdString(sim::getApiInfo(scriptHandle, symbol.toStdString()))};
        return tip;
    };
#ifdef USE_LUA_PARSER
    QStringList symbols;
    QList<int> argIndex;
    int stackHandle = sim::createStack();
    std::string req = "getCallContexts=require'getCallContexts'@";
    try
    {
        sim::executeScriptString(sim_scripttype_sandboxscript, req, stackHandle);
    }
    catch(sim::api_error &ex)
    {
        sim::addLog(sim_verbosity_errors, "failed to execute lua: %s", req);
        return;
    }
    std::string delim = "========================================================";
    std::string code = (boost::format("getCallContexts([%s[%s]%s],%d)@") % delim % input.toStdString().c_str() % delim % (pos+1)).str();
    try
    {
        sim::executeScriptString(sim_scripttype_sandboxscript, code, stackHandle);
    }
    catch(sim::api_error &ex)
    {
        CStackObject *obj = CStackObject::buildItemFromTopStackPosition(stackHandle);
        sim::addLog(sim_verbosity_errors, "failed to execute lua: %s. error: %s", code, obj->toString());
        delete obj;
        return;
    }
    int size = sim::getStackSize(stackHandle);
    if(size == 0)
    {
        sim::addLog(sim_verbosity_debug, "empty result in stack");
        return;
    }
    CStackObject *obj = CStackObject::buildItemFromTopStackPosition(stackHandle);
    if(!obj)
    {
        sim::addLog(sim_verbosity_debug, "CStackObject::buildItemFromTopStackPosition() returned NULL");
        return;
    }
    CStackArray *arr = obj->asArray();
    if(!arr)
    {
        sim::addLog(sim_verbosity_debug, "CStackObject::asArray() returned NULL (CStackObject::getObjectTypeString() == \"%s\")", obj->getObjectTypeString());
        delete obj;
        return;
    }
    for(size_t i = 0; i < arr->getSize(); ++i)
    {
        CStackArray *item = arr->getArray(i);
        if(item)
        {
            QString sym{QString::fromStdString(item->getString(0))};
            int idx{item->getInt(1)};
            symbols << sym;
            argIndex << idx;
        }
    }
    delete obj;
    QStringList calltips;
    for(int i = 0; i < symbols.length(); i++)
    {
        QString txt{getApiInfo(scriptHandle, symbols[i])};
        for(QString tip : txt.split('\n'))
        {
            if(tip == "") break;

            int equalSignPos = tip.indexOf('=');
            int openParenPos = tip.indexOf('(');
            int closeParenPos = tip.lastIndexOf(')');
            if(openParenPos < 0 || closeParenPos < 0)
            {
                sim::addLog(sim_verbosity_errors, "invalid calltip (missing%s%s%s paren): %s",
                        openParenPos < 0 ? " open" : "",
                        openParenPos * closeParenPos > 0 ? " and" : "",
                        closeParenPos < 0 ? " close" : "",
                        tip.toStdString());
                continue;
            }
            QString retArgs, eq, funcName, funcArgs;
            if(equalSignPos >= 0 && equalSignPos < openParenPos)
            {
                eq = " = ";
                retArgs = tip.left(equalSignPos);
                funcName = tip.mid(equalSignPos + 1, openParenPos - equalSignPos - 1);
            }
            else funcName = tip.left(openParenPos);
            funcArgs = tip.mid(openParenPos + 1, closeParenPos - 1 - openParenPos);

            tip = retArgs + eq + funcName;
            QStringList p = funcArgs.split(',');
            for(int j = 0; j < p.length(); j++)
            {
                tip += j == 0 ? "(" : ", ";
                if(argIndex[i] == (j + 1)) tip += "<b>";
                tip += p[j];
                if(argIndex[i] == (j + 1)) tip += "</b>";
            }
            tip += ")";
            calltips << tip;
        }
    }
    emit setCallTip(calltips.join("<br>"));
#else // USE_LUA_PARSER
    // find symbol before '('
    QString symbol = input.left(pos);
    int j = symbol.length() - 1;
    while(j >= 0 && isID(symbol[j])) j--;
    symbol = symbol.mid(j + 1);
    emit setCallTip(getApiInfo(scriptHandle, symbol));
#endif // USE_LUA_PARSER
}

void SIM::setExecWrapper(int scriptHandle, const QString &wrapperFunc)
{
    if(wrapperFunc.isEmpty())
        execWrapper.remove(scriptHandle);
    else
        execWrapper.insert(scriptHandle, wrapperFunc);
}
