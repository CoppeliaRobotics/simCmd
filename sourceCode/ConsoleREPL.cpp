#include "ConsoleREPL.h"
#include <iostream>

#include <simPlusPlus-2/Lib.h>


Readline::Readline(QObject *parent) : QThread(parent)
{
    scriptHandle = -1;

    QThread::setTerminationEnabled(true);
    rx.install_window_change_handler();
    rx.set_max_history_size(128);
    rx.set_max_hint_rows(3);
    rx.set_no_color(false);
    rx.set_word_break_characters(" \n\t,-%!;:=*~^'\"/?<>|[](){}");
    rx.set_completion_count_cutoff(128);
    using namespace std::placeholders;
    rx.set_completion_callback(std::bind(&Readline::hook_completion, this, _1, _2));
}

void Readline::run()
{
    while(!QThread::currentThread()->isInterruptionRequested())
    {
        const char *line = rx.input("> ");
        if(line && *line)
        {
            rx.history_add(line);
            QString line_ = QString::fromUtf8(line);
            emit execCode(scriptHandle, line_);
        }
        else if(!line) // EOF
        {
            std::cout << std::endl;
            sim::quitSimulator();
            break;
        }
    }
}

Replxx::completions_t Readline::hook_completion(const std::string &context, int &contextLen)
{
    QString input = QString::fromStdString(context);
    QStringList completions;
    emit askCompletion(scriptHandle, input, contextLen, &completions);
    Replxx::completions_t ret;
    for(const auto &completion : completions)
        ret.emplace_back(completion.toUtf8().data(), Replxx::Color::DEFAULT);
    return ret;
}

void Readline::setSelectedScript(int newScriptHandle)
{
    if(scriptHandle != newScriptHandle)
    {
        scriptHandle = newScriptHandle;
        sim::addLog(sim_verbosity_warnings, "Selected script: %d", scriptHandle);
    }
}
