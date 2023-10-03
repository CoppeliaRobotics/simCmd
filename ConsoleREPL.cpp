#include "ConsoleREPL.h"
#include <iostream>

#include <simPlusPlus/Lib.h>


Readline::Readline(QObject *parent) : QThread(parent)
{
    sandboxScript = sim::getScriptHandleEx(sim_scripttype_sandboxscript, -1);
    preferredSandboxLang = QString::fromStdString(sim::getStringParam(sim_stringparam_sandboxlang));
    if(preferredSandboxLang == "")
    {
        sim::addLog(sim_verbosity_warnings, "You haven't configured a preferred scripting language for the sandbox.");
        preferredSandboxLang = "Lua";
    }
    printCurrentLanguage();
    sim::addLog(sim_verbosity_warnings, "You can switch it by typing @lua or @python.");
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
            QString line_ = QString::fromUtf8(line);
            if(line_ == "@lua")
            {
                preferredSandboxLang = "Lua";
                printCurrentLanguage();
            }
            else if(line_ == "@py" || line_ == "@python")
            {
                preferredSandboxLang = "Python";
                printCurrentLanguage();
            }
            else
            {
                emit execCode(sandboxScript, "@" + preferredSandboxLang.toLower(), line_);
            }
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
    int pos = context.size() - contextLen;
    //auto c2 = context.substr(context.size() - contextLen);
    QStringList completions;
    emit askCompletion(sim_scripttype_sandboxscript, "", input, pos, &completions);
    Replxx::completions_t ret;
    for(const auto &completion : completions)
        ret.emplace_back(completion.toUtf8().data(), Replxx::Color::DEFAULT);
    return ret;
}

void Readline::printCurrentLanguage()
{
    sim::addLog(sim_verbosity_warnings, "Sandbox language: %s", preferredSandboxLang.toStdString());
}
