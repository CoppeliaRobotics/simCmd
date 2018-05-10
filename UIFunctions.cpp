#include "UIFunctions.h"
#include "debug.h"
#include "UIProxy.h"
#include "stubs.h"
#include "v_repLib.h"
#include <stdexcept>
#include <iomanip>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <QRegExp>

// UIFunctions is a singleton

UIFunctions *UIFunctions::instance = NULL;

UIFunctions::UIFunctions(QObject *parent)
    : QObject(parent)
{
    connectSignals();
    initStringRenderingFlags();
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
    connect(uiproxy, &UIProxy::clearHistory, this, &UIFunctions::clearHistory);
}

QStringList loadHistoryData()
{
    simInt histSize;
    simChar *pdata = simPersistentDataRead("LuaCommanderHistory", &histSize);
    QStringList hist;
    if(pdata)
    {
        QString s = QString::fromUtf8(pdata);
        hist = s.split(QRegExp("(\\r\\n)|(\\n\\r)|\\r|\\n"), QString::SkipEmptyParts);
        simReleaseBuffer(pdata);
    }
    return hist;
}

void saveHistoryData(QStringList hist)
{
    QString histStr = hist.join("\n");
    simPersistentDataWrite("LuaCommanderHistory", histStr.toUtf8(), histStr.length() + 1, 1);
}

void UIFunctions::loadHistory()
{
    QStringList hist = loadHistoryData();
    emit historyChanged(hist);
}

void UIFunctions::clearHistory()
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

void UIFunctions::appendHistory(QString cmd)
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

void UIFunctions::setOptions(const PersistentOptions &options)
{
    this->options = options;
}

static inline bool isSpecialChar(char c)
{
    if(c >= 'a' && c <= 'z') return false;
    if(c >= 'A' && c <= 'Z') return false;
    if(c >= '0' && c <= '9') return false;
    static const char *allowed = " !@#$%^&*()_-+=[]{}\\|:;\"'<>,./?`~\n\r\t";
    for(int i = 0; i < strlen(allowed); i++)
        if(c == allowed[i]) return false;
    return true;
}

std::string UIFunctions::getStackTopAsString(int stackHandle, const PersistentOptions &opts, int depth, bool quoteStrings, bool insideTable, std::string *strType)
{
    if(opts.mapMaxDepth >= 0 && depth > opts.mapMaxDepth)
        return "<...>";

    simBool boolValue;
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
                std::string key = getStackTopAsString(stackHandle, opts, depth + 1, false, true);

                simMoveStackItemToTop(stackHandle, oldSize - 1);
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

        simPopStackItem(stackHandle, 1);
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
        std::ostringstream ss;
        ss << std::setprecision(opts.floatPrecision) << doubleValue;
        return ss.str();
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
                else if(c < 32 || c > 126)
                    //ss << "\\x" << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(c);
                    ss << '?';
                else if(c == '\\')
                    ss << "\\\\";
                else
                    ss << c;
            }
            s = ss.str();
        }

        simReleaseBuffer(stringValue);

        if(quoteStrings)
        {
            boost::replace_all(s, "\"", "\\\"");
            return "\"" + s + "\"";
        }
        else return s;
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

void UIFunctions::initStringRenderingFlags()
{
    stringRenderingFlags.clear();
    stringRenderingFlags << "sort";
    stringRenderingFlags << "precision";
    stringRenderingFlags << "depth";
    stringRenderingFlags << "escape";
}

QStringList UIFunctions::getMatchingStringRenderingFlags(QString shortFlag)
{
    QStringList ret;
    foreach(const QString &flag, stringRenderingFlags)
    {
        if(flag.startsWith(shortFlag))
            ret << flag;
    }
    return ret;
}

void UIFunctions::parseStringRenderingFlags(PersistentOptions *popts, const QString &code)
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
            if(p < 0 || p > 20)
                throw std::runtime_error((boost::format("invalid 'precision' option: '%d' (should be between 0 and 20)") % optVal.toStdString()).str());
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

void UIFunctions::onExecCode(QString code, int scriptHandleOrType, QString scriptName)
{
    ASSERT_THREAD(!UI);

    appendHistory(code);

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

    PersistentOptions opts = options;
    try
    {
        parseStringRenderingFlags(&opts, code);
    }
    catch(std::exception &ex)
    {
        simAddStatusbarMessage((boost::format("LuaCommander: warning: %s") % ex.what()).str().c_str());
    }

    simInt ret = simExecuteScriptString(scriptHandleOrType, s1.data(), stackHandle);
    if(ret != 0)
    {
        simAddStatusbarMessage(getStackTopAsString(stackHandle, opts).c_str());
    }
    else
    {
        simInt size = simGetStackSize(stackHandle);
        if(size > 0)
        {
            simAddStatusbarMessage(getStackTopAsString(stackHandle, opts).c_str());
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
    if(i >= 0 && i < cl.size() && cl[i] == prefix)
        i++;
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

