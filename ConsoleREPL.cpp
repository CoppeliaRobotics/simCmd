#include "ConsoleREPL.h"
#include <iostream>

#include <simPlusPlus/Lib.h>


Readline::Readline(QObject *parent) : QThread(parent)
{
    scriptHandle = -1;
    sandboxScript = sim::getScriptHandleEx(sim_scripttype_sandboxscript, -1);
    preferredSandboxLang = QString::fromStdString(sim::getStringParam(sim_stringparam_sandboxlang));
    if(preferredSandboxLang == "")
    {
        preferredSandboxLang = "Lua";
        sim::addLog(sim_verbosity_warnings, "You haven't configured a preferred scripting language for the sandbox. Using %s.", preferredSandboxLang.toStdString());
    }
    setSelectedScript(-1, preferredSandboxLang);

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
                if(scriptHandle == sandboxScript)
                    setSelectedScript(-1, "Lua");
                else
                    sim::addLog(sim_verbosity_errors, "Only works when sandbox script is selected.");
            }
            else if(line_ == "@py" || line_ == "@python")
            {
                if(scriptHandle == sandboxScript)
                    setSelectedScript(-1, "Python");
                else
                    sim::addLog(sim_verbosity_errors, "Only works when sandbox script is selected.");
            }
            else
            {
                emit execCode(scriptHandle, "@" + lang.toLower(), line_);
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
    QStringList completions;
    QString langSuffix;
    if(lang != "") langSuffix = "@" + lang.toLower();
    emit askCompletion(scriptHandle, langSuffix, input, contextLen, &completions);
    Replxx::completions_t ret;
    for(const auto &completion : completions)
        ret.emplace_back(completion.toUtf8().data(), Replxx::Color::DEFAULT);
    return ret;
}

void Readline::setSelectedScript(int newScriptHandle, QString newLang)
{
    if(newScriptHandle == -1)
        newScriptHandle = sandboxScript;
    if(newScriptHandle == sandboxScript && newLang == "")
        newLang = preferredSandboxLang;

    if(scriptHandle != newScriptHandle || newLang.toLower() != lang.toLower())
    {
        scriptHandle = newScriptHandle;
        lang = newLang.left(1).toUpper() + newLang.mid(1).toLower();
        if(newScriptHandle == sandboxScript)
            sim::addLog(sim_verbosity_warnings, "Selected script: Sandbox (%s)", lang.toStdString());
        else
            sim::addLog(sim_verbosity_warnings, "Selected script: %d (%s)", scriptHandle, lang.toStdString());
    }
}
