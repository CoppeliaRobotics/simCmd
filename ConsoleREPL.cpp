#include "ConsoleREPL.h"
#include <iostream>

#include <simPlusPlus/Lib.h>


Readline::Readline(QObject *parent) : QThread(parent)
{
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
        const char *line = rx.input("coppeliaSim> ");
        if(line && *line)
        {
            rx.history_add(line);
            emit execCode(QString::fromUtf8(line), sim_scripttype_sandboxscript, "");
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
    auto c2 = context.substr(context.size() - contextLen);
    QStringList completions;
    emit askCompletion(sim_scripttype_sandboxscript, "", QString::fromStdString(c2), 'i', &completions);
    Replxx::completions_t ret;
    for(const auto &completion : completions)
        ret.emplace_back(completion.toUtf8().data(), Replxx::Color::DEFAULT);
    return ret;
}
