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
#include <simPlusPlus-2/Plugin.h>
#include "plugin.h"
#include "stubs.h"
#include "config.h"
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QSplitter>
#include "qcommanderwidget.h"
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

        if(sim::getIntProperty(sim_handle_app, "headlessMode"))
        {
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
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        layout->setMargin(0);
#endif
        layout->setContentsMargins(0, 0, 0, 0);
        splitterChild->setLayout(layout);
        commanderWidget = new QCommanderWidget();
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
        auto getScriptLabel = [](int type, int scriptHandle, const QString &name) -> QString {
            QString typePrefix = "";
            if(type == sim_scripttype_simulation) typePrefix = "Simulation script ";
            if(type == sim_scripttype_customization) typePrefix = "Customization script ";
            std::string lang = sim::getStringProperty(scriptHandle, "language");
            return QString("%1'%2' (%3)").arg(typePrefix, name, QString::fromStdString(lang));
        };
        static bool previouslyRunning {false};
        bool isRunning = sim::getSimulationState() == sim_simulation_advancing_running;
        int sandboxScript = sim::getScriptHandleEx(sim_scripttype_sandbox, -1);
        int mainScript = sim::getScriptHandleEx(sim_scripttype_main, -1);
        QMap<int, QString> childScripts;
        QMap<int, QString> customizationScripts;
        for(int scriptHandle : sim::getObjects(sim_sceneobject_script))
        {
            QString name = QString::fromStdString(sim::getObjectAlias(scriptHandle, 5));
            int scriptType = sim::getIntProperty(scriptHandle, "scriptType");
            if(scriptType == sim_scripttype_simulation && isRunning)
                childScripts[scriptHandle] = getScriptLabel(sim_scripttype_simulation, scriptHandle, name);
            else if(scriptType == sim_scripttype_customization)
                customizationScripts[scriptHandle] = getScriptLabel(sim_scripttype_customization, scriptHandle, name);
        }
        bool havePython = !*sim::getBoolProperty(sim_handle_app, "signal.pythonSandboxInitFailed", false);
        if(sim::getStringProperty(sim_handle_app, "sandboxLang") == "bareLua")
            havePython = false;
        SIM::getInstance()->scriptListChanged(sandboxScript, mainScript, childScripts, customizationScripts, isRunning, isRunning ^ previouslyRunning, havePython);
        previouslyRunning = isRunning;
    }

    void updateWidgetOptions()
    {
        SIM *sim = SIM::getInstance();
        sim->setPreferredSandboxLang(
            QString::fromStdString(sim::getStringProperty(sim_handle_app, "sandboxLang"))
        );
        sim->setAutoAcceptCommonCompletionPrefix(
            *sim::getBoolProperty(sim_handle_app, "customData.simCmd.autoAcceptCommonCompletionPrefix", true)
        );
        sim->setShowMatchingHistory(
            *sim::getBoolProperty(sim_handle_app, "customData.simCmd.showMatchingHistory", false)
        );
    }

    void onInstancePass(const sim::InstancePassFlags &flags) override
    {
        if(sim::getIntProperty(sim_handle_app, "headlessMode"))
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
            QObject::connect(commanderWidget, &QCommanderWidget::execCode, sim, &SIM::onExecCode);
            QObject::connect(commanderWidget, &QCommanderWidget::askCompletion, sim, &SIM::onAskCompletion);
            QObject::connect(commanderWidget, &QCommanderWidget::askCallTip, sim, &SIM::onAskCallTip);
            QObject::connect(commanderWidget, &QCommanderWidget::addLog, sim, &SIM::addLog);
            QObject::connect(sim, &SIM::setVisible, commanderWidget, &QCommanderWidget::setVisible);
            QObject::connect(sim, &SIM::scriptListChanged, commanderWidget, &QCommanderWidget::onScriptListChanged);
            QObject::connect(sim, &SIM::setCompletion, commanderWidget, &QCommanderWidget::onSetCompletion);
            QObject::connect(sim, &SIM::setCallTip, commanderWidget, &QCommanderWidget::onSetCallTip);
            QObject::connect(sim, &SIM::historyChanged, commanderWidget, &QCommanderWidget::setHistory);
            QObject::connect(sim, &SIM::setPreferredSandboxLang, commanderWidget, &QCommanderWidget::setPreferredSandboxLang);
            QObject::connect(sim, &SIM::setAutoAcceptCommonCompletionPrefix, commanderWidget, &QCommanderWidget::setAutoAcceptCommonCompletionPrefix);
            QObject::connect(sim, &SIM::setShowMatchingHistory, commanderWidget, &QCommanderWidget::setShowMatchingHistory);
            QObject::connect(sim, &SIM::setSelectedScript, commanderWidget, &QCommanderWidget::setSelectedScript);
            sim->loadHistory();
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

    void setSelectedScript(setSelectedScript_in *in, setSelectedScript_out *out)
    {
        QString lang = QString::fromStdString(in->lang);
        if(sim::getIntProperty(sim_handle_app, "headlessMode"))
            readline->setSelectedScript(in->scriptHandle, lang);
        else
            SIM::getInstance()->setSelectedScript(in->scriptHandle, lang, false, false);
    }

    void toggleStatusbarHeight(toggleStatusbarHeight_in *in, toggleStatusbarHeight_out *out)
    {
        SIM::getInstance()->toggleStatusbarHeight();
    }

private:
    Readline *readline{nullptr};
    bool firstInstancePass = true;
    QPlainTextEdit *statusBar;
    QSplitter *splitter = 0L;
    QWidget *splitterChild = 0L;
    QVBoxLayout *layout = 0L;
    QCommanderWidget *commanderWidget = 0L;
};

SIM_UI_PLUGIN(Plugin)
#include "stubsPlusPlus.cpp"
