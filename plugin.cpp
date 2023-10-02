#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "SIM.h"
#include "UI.h"
#include <simPlusPlus/Plugin.h>
#include "plugin.h"
#include "stubs.h"
#include "config.h"
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QSplitter>
#include "qluacommanderwidget.h"
#include "ConsoleREPL.h"

class Plugin : public sim::Plugin
{
public:
    void onInit() override
    {
        if(!registerScriptStuff())
            throw std::runtime_error("failed to register script stuff");

        setExtVersion("Lua REPL (read-eval-print-loop) Plugin");
        setBuildDate(BUILD_DATE);

        SIM::getInstance(); // construct SIM here (SIM thread)

        if(sim::getBoolParam(sim_boolparam_headless))
        {
            sim::addLog(sim_verbosity_loadinfos, "running in headless mode, using libreadline");
            auto sim = SIM::getInstance();
            readline = new Readline(sim);
            QObject::connect(readline, &Readline::execCode, sim, &SIM::onExecCode, Qt::BlockingQueuedConnection);
            QObject::connect(readline, &Readline::askCompletion, sim, &SIM::onAskCompletion, Qt::BlockingQueuedConnection);
            //readline->start(); // start it on first instance pass, so the prompt is clear
        }
    }

    void onCleanup() override
    {
        if(readline)
        {
            //readline->requestInterruption();
            readline->terminate();
            QThread::sleep(1);
        }

        SIM::destroyInstance();
        SIM_THREAD = NULL;
    }

    void onUIInit() override
    {
        UI::getInstance(); // construct UI here (UI thread)

        // find the StatusBar widget (QPlainTextEdit)
        statusBar = UI::simMainWindow->findChild<QPlainTextEdit*>("statusBar");
        if(!statusBar)
            throw std::runtime_error("cannot find the statusbar widget");

        // attach widget to CoppeliaSim main window
        splitter = (QSplitter*)statusBar->parentWidget();
        UI::getInstance()->setStatusBar(statusBar, splitter);
        splitterChild = new QWidget();
        splitter->replaceWidget(1,splitterChild);
        layout = new QVBoxLayout();
        layout->setSpacing(0);
        layout->setMargin(0);
        layout->setContentsMargins(0,0,0,0);
        splitterChild->setLayout(layout);
        commanderWidget = new QLuaCommanderWidget();
        layout->addWidget(statusBar);
        layout->addWidget(commanderWidget);
        splitterChild->setMaximumHeight(600);

        updateUI();
    }

    void onUICleanup() override
    {
        if(commanderWidget)
        {
            layout->removeWidget(statusBar);
            delete splitter->replaceWidget(1, statusBar);
        }

        UI::destroyInstance();
        UI_THREAD = NULL;
    }

    void updateUI()
    {
        if(!commanderWidget) return;

        bool oldVis = commanderWidget->isVisible();

        if(!firstInstancePass)
            commanderWidget->setVisible(true /*options.enabled*/);

        bool newVis = commanderWidget->isVisible();
        if(oldVis && !newVis)
        {
            // when commander is hidden, focus the statusbar
            statusBar->setFocus();
        }
        else if(!oldVis && newVis)
        {
            // when it is shown, focus it
            commanderWidget->editor_()->setFocus();
        }
    }

    void updateScriptsList()
    {
        bool isRunning = sim::getSimulationState() == sim_simulation_advancing_running;
        int sandboxScript = sim::getScriptHandleEx(sim_scripttype_sandboxscript, -1);
        int mainScript = sim::getScriptHandleEx(sim_scripttype_mainscript, -1);
        QMap<int, QString> childScripts;
        QMap<int, QString> customizationScripts;
        for(int objectHandle : sim::getObjects(sim_handle_all))
        {
            QString name = QString::fromStdString(sim::getObjectAlias(objectHandle, 5));
            int scriptHandle;
            if(isRunning)
            {
                scriptHandle = sim::getScriptHandleEx(sim_scripttype_childscript, objectHandle);
                if(scriptHandle != -1)
                    childScripts[scriptHandle] = name;
            }
            scriptHandle = sim::getScriptHandleEx(sim_scripttype_customizationscript, objectHandle);
            if(scriptHandle != -1)
                customizationScripts[scriptHandle] = name;
        }
        SIM::getInstance()->scriptListChanged(sandboxScript, mainScript, childScripts, customizationScripts, isRunning);
    }

    void updateWidgetOptions()
    {
        SIM *sim = SIM::getInstance();
        sim->setResizeStatusbarWhenFocused(
            sim::getNamedBoolParam("simLuaCmd.resizeStatusbarWhenFocused").value_or(false)
        );
        sim->setPreferredSandboxLang(
            QString::fromStdString(sim::getNamedStringParam("simLuaCmd.preferredSandboxLang").value_or(""))
        );
        sim->setAutoAcceptCommonCompletionPrefix(
            sim::getNamedBoolParam("simLuaCmd.autoAcceptCommonCompletionPrefix").value_or(true)
        );
        sim->setShowMatchingHistory(
            sim::getNamedBoolParam("simLuaCmd.showMatchingHistory").value_or(false)
        );
    }

    void onInstancePass(const sim::InstancePassFlags &flags) override
    {
        if(sim::getBoolParam(sim_boolparam_headless))
        {
            // instance pass for headless here
            if(firstInstancePass)
            {
                readline->start();
                firstInstancePass = false;
            }
            return;
        }

        // instance pass for ui:

        if(!commanderWidget) return;

        if(firstInstancePass)
        {
            int id = qRegisterMetaType< QMap<int,QString> >();

            SIM *sim = SIM::getInstance();
            QObject::connect(commanderWidget, &QLuaCommanderWidget::execCode, sim, &SIM::onExecCode);
            QObject::connect(commanderWidget, &QLuaCommanderWidget::askCompletion, sim, &SIM::onAskCompletion);
            QObject::connect(commanderWidget, &QLuaCommanderWidget::askCallTip, sim, &SIM::onAskCallTip);
            QObject::connect(sim, &SIM::setVisible, commanderWidget, &QLuaCommanderWidget::setVisible);
            QObject::connect(sim, &SIM::scriptListChanged, commanderWidget, &QLuaCommanderWidget::onScriptListChanged);
            QObject::connect(sim, &SIM::setCompletion, commanderWidget, &QLuaCommanderWidget::onSetCompletion);
            QObject::connect(sim, &SIM::setCallTip, commanderWidget, &QLuaCommanderWidget::onSetCallTip);
            QObject::connect(sim, &SIM::historyChanged, commanderWidget, &QLuaCommanderWidget::setHistory);
            QObject::connect(sim, &SIM::setResizeStatusbarWhenFocused, commanderWidget, &QLuaCommanderWidget::setResizeStatusbarWhenFocused);
            QObject::connect(sim, &SIM::setPreferredSandboxLang, commanderWidget, &QLuaCommanderWidget::setPreferredSandboxLang);
            QObject::connect(sim, &SIM::setAutoAcceptCommonCompletionPrefix, commanderWidget, &QLuaCommanderWidget::setAutoAcceptCommonCompletionPrefix);
            QObject::connect(sim, &SIM::setShowMatchingHistory, commanderWidget, &QLuaCommanderWidget::setShowMatchingHistory);
            sim->loadHistory();
        }

        if(commanderWidget->closeFlag.load())
        {
            commanderWidget->closeFlag.store(false);
            //options.enabled = false;
            updateUI();
        }

        updateWidgetOptions();

        if(firstInstancePass || flags.objectsErased || flags.objectsCreated || flags.modelLoaded || flags.sceneLoaded || flags.undoCalled || flags.redoCalled || flags.sceneSwitched || flags.scriptCreated || flags.scriptErased || flags.simulationStarted || flags.simulationEnded)
        {
            updateScriptsList();
        }

        firstInstancePass = false;
    }

    void setVisible(setVisible_in *in, setVisible_out *out)
    {
        SIM::getInstance()->setVisible(in->b);
    }

    void clearHistory(clearHistory_in *in, clearHistory_out *out)
    {
        SIM::getInstance()->clearHistory();
    }

    void setExecWrapper(setExecWrapper_in *in, setExecWrapper_out *out)
    {
        SIM::getInstance()->setExecWrapper(in->_.scriptID, QString::fromStdString(in->wrapperFunc));
    }

private:
    Readline *readline{nullptr};
    bool firstInstancePass = true;
    QPlainTextEdit *statusBar;
    QSplitter *splitter = 0L;
    QWidget *splitterChild = 0L;
    QVBoxLayout *layout = 0L;
    QLuaCommanderWidget *commanderWidget = 0L;
};

SIM_UI_PLUGIN(Plugin)
#include "stubsPlusPlus.cpp"
