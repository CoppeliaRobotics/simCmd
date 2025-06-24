#include "SIM.h"
#include "UI.h"
#include "stubs.h"
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
#include <QRegularExpression>
#include <QCborArray>
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
        auto pdata = sim::getBufferProperty(sim_handle_app, "customData.simCmd.history", {});
        if(!pdata) return hist;
        auto pdataBA = QByteArray::fromStdString(*pdata);
        QCborParserError parserError;
        QCborValue value = QCborValue::fromCbor(pdataBA, &parserError);
        if(parserError.error != QCborError::NoError)
        {
            sim::addLog(sim_verbosity_warnings, "customData.simCmd.history contains invalid CBOR data: " + parserError.errorString().toStdString());
            return hist;
        }
        if(!value.isArray())
        {
            sim::addLog(sim_verbosity_warnings, "customData.simCmd.history is not a CBOR array (type = " + std::to_string(value.type()) + ")");
            return hist;
        }
        QCborArray array = value.toArray();
        for(const QCborValue &item : array)
        {
            if(item.isString())
                hist.append(item.toString());
            else
                sim::addLog(sim_verbosity_warnings, "customData.simCmd.history item is not a CBOR string");
        }
    }
    catch(sim::api_error &ex)
    {
        sim::addLog(sim_verbosity_debug, "failed to read history");
    }
    return hist;
}

void saveHistoryData(QStringList hist)
{
    QCborArray array;
    for(const QString &histItem : hist)
        array.append(histItem);
    QByteArray histData = array.toCborValue().toCbor();
    try
    {
        sim::setBufferProperty(sim_handle_app, "customData.simCmd.history", histData.toStdString());
    }
    catch(sim::api_error &ex)
    {
        sim::addLog(sim_verbosity_debug, "failed to write history");
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

    bool historySkipRepeated = *sim::getBoolProperty(sim_handle_app, "customData.simCmd.historySkipRepeated", true);
    bool historyRemoveDups = *sim::getBoolProperty(sim_handle_app, "customData.simCmd.historyRemoveDups", false);
    int historySize = *sim::getIntProperty(sim_handle_app, "customData.simCmd.historySize", 1000);

    if(!historySkipRepeated || hist.isEmpty() || hist[hist.size() - 1] != cmd)
        hist << cmd;

    if(historyRemoveDups)
    {
        reverse(hist);
        hist.removeDuplicates();
        reverse(hist);
    }

    if(historySize >= 0)
    {
        int numToRemove = hist.size() - historySize;
        if(numToRemove > 0)
            hist.erase(hist.begin(), hist.begin() + numToRemove);
    }

    saveHistoryData(hist);

    emit historyChanged(hist);
}

void SIM::addLog(int verbosity, QString message)
{
    sim::addLog(verbosity, message.toStdString());
}

void SIM::onExecCode(int scriptHandle, QString lang, QString code)
{
    ASSERT_THREAD(!UI);

    appendHistory(code);

    if(!sim::getIntProperty(sim_handle_app, "headlessMode"))
        sim::addLog(sim_verbosity_msgs|sim_verbosity_undecorated, "> %s", code.toStdString());

    try
    {
        int stackHandle = sim::createStack();
        QString ewFunc = "_evalExec";
        auto i = execWrapper.find(scriptHandle);
        if(i != execWrapper.end()) ewFunc = i.value();
        sim::pushStringOntoStack(stackHandle, code.toStdString());
        if(lang != "")
            ewFunc += "@" + lang.toLower();
        sim::callScriptFunctionEx(scriptHandle, ewFunc.toStdString(), stackHandle);
        sim::releaseStack(stackHandle);
    }
    catch(std::exception &ex)
    {
        sim::addLog(sim_verbosity_errors, "Code evaluation failed.");
    }

    sim::announceSceneContentChange();
}

void SIM::onAskCompletion(int scriptHandle, QString lang, QString input, int pos, QStringList *clout)
{
    ASSERT_THREAD(!UI);

    QStringList cl;
    int stackHandle = sim::createStack();
    writeToStack(input.toStdString(), stackHandle);
    writeToStack(pos, stackHandle);
    try
    {
        QString func = "_getCompletion";
        if(lang != "")
            func += "@" + lang.toLower();
        sim::callScriptFunctionEx(scriptHandle, func.toStdString(), stackHandle);
        std::vector<std::string> r;
        readFromStack(stackHandle, &r);

        for(const auto &x : r)
            cl << QString::fromStdString(x);
        cl.sort();
    }
    catch(std::exception &ex) {}
    sim::releaseStack(stackHandle);

    if(clout)
    {
        // clout used by replxx, wants also the context in the completion
        for(const QString &s : cl)
            *clout << (input + s);
    }

    emit setCompletion(cl);
}

void SIM::onAskCallTip(int scriptHandle, QString lang, QString input, int pos)
{
    int stackHandle = sim::createStack();
    writeToStack(input.toStdString(), stackHandle);
    writeToStack(pos, stackHandle);
    try
    {
        QString func = "_getCalltip";
        if(lang != "")
            func += "@" + lang.toLower();
        sim::callScriptFunctionEx(scriptHandle, func.toStdString(), stackHandle);
        std::string r;
        readFromStack(stackHandle, &r);
        emit setCallTip(QString::fromStdString(r));
    }
    catch(sim::exception &ex) {}
    sim::releaseStack(stackHandle);
}
