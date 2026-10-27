#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <jsoncons/json.hpp>
#include <QString>
#include <QMetaType>
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QSplitter>
#include <simPlusPlus-2/Plugin.h>
#include "SIM.h"
#include "UI.h"
#include "plugin.h"
#include "stubs.h"
#include "config.h"
#include "qcommanderwidget.h"
#include "ConsoleREPL.h"
#include "common.h"

Q_DECLARE_METATYPE(sim::handle_t)
Q_DECLARE_METATYPE(HandleStringMap)

using json = jsoncons::json;

class Plugin : public sim::Plugin
{
public:
    void onInit() override
    {
        if(!registerScriptStuff())
            throw std::runtime_error("failed to register script stuff");

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

    void onEvent(const sim::EventInfo &info, const json &data) override
    {
        if(info.event == "objectChanged" && allScripts.contains(info.handle) && data.contains("state"))
        {
            updateScriptsList();
        }
    }

    void onScriptStateAboutToBeDestroyed(sim::handle_t scriptHandle, long long scriptUid) override
    {
        updateScriptsList();
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

    void updateScriptsList(bool immediate = false)
    {
        if(!immediate)
        {
            updateScriptListPending = true;
            return;
        }

        auto getScriptLabel = [](sim::handle_t nakedHandle, const QString &name = "") -> QString {
            static const QMap<QString, QString> typeLabel {
                {"sandbox", "Sandbox script"},
                {"main", "Main script"},
                {"simulation", "Simulation script: "},
                {"customization", "Customization script: "},
                {"addon", "Add-on: "},
            };
            QString type = QString::fromStdString(sim::getStringProperty(nakedHandle, "script.type"));
            QString lang = QString::fromStdString(sim::getStringProperty(nakedHandle, "language"));
            QString ret = typeLabel[type];
            if(name != "")
                ret += QString("'%1'").arg(name);
            ret += QString(" (%1)").arg(lang);
            return ret;
        };
        static bool previouslyRunning {false};
        bool isRunning = sim::getSimulationState() == sim_simulation_advancing_running;
        QMap<sim::handle_t, QString> sandboxScripts, sandboxLang;
        for(auto n : std::vector<std::string>{"sandboxLua", "sandboxPython"})
        {
            sim::handle_t sandboxScript = sim::getHandleProperty(sim_handle_app, n);
            if(sandboxScript == -1) continue;
            sandboxScripts[sandboxScript] = getScriptLabel(sandboxScript);
            sandboxLang[sandboxScript] = QString::fromStdString(sim::getStringProperty(sandboxScript, "language"));
            allScripts.insert(sandboxScript, true);
        }
        sim::handle_t mainScript = sim::getHandleProperty(sim_handle_scene, "mainScript");
        allScripts.insert(mainScript, true);
        QMap<sim::handle_t, QString> simulationScripts;
        QMap<sim::handle_t, QString> customizationScripts;
        for(sim::handle_t scriptHandle : sim::getObjects(sim_sceneobject_script))
        {
            QString name = QString::fromStdString(sim::getObjectAlias(scriptHandle, 5));
            sim::handle_t nakedScriptHandle = sim::getHandleProperty(scriptHandle, "script");
            allScripts.insert(nakedScriptHandle, true);
            int state = sim::getIntProperty(nakedScriptHandle, "script.state");
            if(state != sim_scriptstate_initialized) continue;
            QString type = QString::fromStdString(sim::getStringProperty(nakedScriptHandle, "type"));
            if(type == "simulation")
                simulationScripts[scriptHandle] = getScriptLabel(scriptHandle, name);
            else if(type == "customization")
                customizationScripts[scriptHandle] = getScriptLabel(scriptHandle, name);
        }
        QMap<sim::handle_t, QString> addons;
        for(sim::handle_t addonHandle : sim::getHandleArrayProperty(sim_handle_app, "addOns"))
        {
            allScripts.insert(addonHandle, true);
            int state = sim::getIntProperty(addonHandle, "state");
            if(state != sim_scriptstate_initialized) continue;
            QString name = QString::fromStdString(sim::getStringProperty(addonHandle, "addOnMenuPath"));
            addons[addonHandle] = getScriptLabel(addonHandle, name);
        }
        QString preferredSandboxLang = QString::fromStdString(*sim::getStringProperty(sim_handle_app, "customData.simCmd.sandboxLang", "lua"));
        SIM::getInstance()->scriptListChanged(sandboxScripts, sandboxLang, preferredSandboxLang, mainScript, simulationScripts, customizationScripts, addons, isRunning, isRunning ^ previouslyRunning);
        previouslyRunning = isRunning;
    }

    void updateWidgetOptions()
    {
        SIM *sim = SIM::getInstance();
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
            qRegisterMetaType<sim::handle_t>("sim::handle_t");
            qRegisterMetaType<HandleStringMap>("HandleStringMap");

            SIM *sim = SIM::getInstance();
            QObject::connect(commanderWidget, &QCommanderWidget::execCode, sim, &SIM::onExecCode);
            QObject::connect(commanderWidget, &QCommanderWidget::askCompletion, sim, &SIM::onAskCompletion);
            QObject::connect(commanderWidget, &QCommanderWidget::askCallTip, sim, &SIM::onAskCallTip);
            QObject::connect(commanderWidget, &QCommanderWidget::addLog, sim, &SIM::addLog);
            QObject::connect(commanderWidget, &QCommanderWidget::currentScriptChanged, sim, &SIM::onCurrentScriptChanged);
            QObject::connect(sim, &SIM::setVisible, commanderWidget, &QCommanderWidget::setVisible);
            QObject::connect(sim, &SIM::scriptListChanged, commanderWidget, &QCommanderWidget::onScriptListChanged);
            QObject::connect(sim, &SIM::setCompletion, commanderWidget, &QCommanderWidget::onSetCompletion);
            QObject::connect(sim, &SIM::setCallTip, commanderWidget, &QCommanderWidget::onSetCallTip);
            QObject::connect(sim, &SIM::historyChanged, commanderWidget, &QCommanderWidget::setHistory);
            QObject::connect(sim, &SIM::setAutoAcceptCommonCompletionPrefix, commanderWidget, &QCommanderWidget::setAutoAcceptCommonCompletionPrefix);
            QObject::connect(sim, &SIM::setShowMatchingHistory, commanderWidget, &QCommanderWidget::setShowMatchingHistory);
            QObject::connect(sim, &SIM::setSelectedScript, commanderWidget, &QCommanderWidget::setSelectedScript);
            sim->loadHistory();
        }

        updateWidgetOptions();

        if(updateScriptListPending || firstInstancePass || flags.objectsErased || flags.objectsCreated || flags.modelLoaded || flags.sceneLoaded || flags.undoCalled || flags.redoCalled || flags.sceneSwitched || flags.scriptCreated || flags.scriptErased || flags.simulationStarted || flags.simulationEnded)
        {
            updateScriptsList(true);
            updateScriptListPending = false;
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
        if(sim::getIntProperty(sim_handle_app, "headlessMode"))
            readline->setSelectedScript(in->scriptHandle);
        else
            SIM::getInstance()->setSelectedScript(in->scriptHandle, false, false);
    }

    void toggleStatusbarHeight(toggleStatusbarHeight_in *in, toggleStatusbarHeight_out *out)
    {
        SIM::getInstance()->toggleStatusbarHeight();
    }

    void exec(exec_in *in, exec_out *out)
    {
        sim::handle_t sandboxScript = sim::getScriptHandleEx(sim_scripttype_sandbox, -1);
        QString code = QString::fromStdString(in->code);
        SIM::getInstance()->onExecCode(sandboxScript, code);
    }

private:
    Readline *readline{nullptr};
    bool firstInstancePass = true;
    QPlainTextEdit *statusBar;
    QSplitter *splitter = 0L;
    QWidget *splitterChild = 0L;
    QVBoxLayout *layout = 0L;
    QCommanderWidget *commanderWidget = 0L;
    QMap<sim::handle_t, bool> allScripts;
    bool updateScriptListPending = false;
};

SIM_UI_PLUGIN(Plugin)
#include "stubsPlusPlus.cpp"
