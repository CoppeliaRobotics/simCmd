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
#include <simStubsGen/cpp/common.h>

// SIM is a singleton

SIM *SIM::instance = NULL;

SIM::SIM(QObject *parent)
    : QObject(parent)
{
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

void SIM::onAskCompletion(int scriptHandle, QString input, int pos, QString token, QChar context, QStringList *clout)
{
    ASSERT_THREAD(!UI);

    QStringList cl = getCompletion(scriptHandle, input, pos, token, context);
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

void SIM::onExecCode(QString code, int scriptHandle)
{
    ASSERT_THREAD(!UI);

    appendHistory(code);

    if(!sim::getBoolParam(sim_boolparam_headless))
        sim::addLog(sim_verbosity_msgs|sim_verbosity_undecorated, "> %s", code.toStdString());

    PersistentOptions opts = options;

    int stackHandle = sim::createStack();
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
    sim::releaseStack(stackHandle);

    sim::announceSceneContentChange();
}

QStringList SIM::getCompletion(int scriptHandle, QString input, int pos, QString word, QChar context)
{
    QStringList result;
    int stackHandle = sim::createStack();
    writeToStack(input.toStdString(), stackHandle);
    writeToStack(pos, stackHandle);
    try
    {
        sim::callScriptFunctionEx(scriptHandle, "_getCompletion", stackHandle);
        std::vector<std::string> r;
        readFromStack(stackHandle, &r);

        for(const auto &x : r)
            result << QString::fromStdString(x);
        result.sort();
    }
    catch(std::exception &ex) {}
    sim::releaseStack(stackHandle);
    return result;
}

void SIM::onAskCallTip(int scriptHandle, QString input, int pos)
{
    int stackHandle = sim::createStack();
    writeToStack(input.toStdString(), stackHandle);
    writeToStack(pos, stackHandle);
    try
    {
        sim::callScriptFunctionEx(scriptHandle, "_getCalltip", stackHandle);
        std::string r;
        readFromStack(stackHandle, &r);
        emit setCallTip(QString::fromStdString(r));
    }
    catch(sim::exception &ex) {}
    sim::releaseStack(stackHandle);
}

void SIM::setExecWrapper(int scriptHandle, const QString &wrapperFunc)
{
    if(wrapperFunc.isEmpty())
        execWrapper.remove(scriptHandle);
    else
        execWrapper.insert(scriptHandle, wrapperFunc);
}
