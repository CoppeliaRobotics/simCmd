#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include "UIFunctions.h"
#include "UIProxy.h"
#include "v_repPlusPlus/Plugin.h"
#include "plugin.h"
#include "stubs.h"
#include "config.h"
#include "debug.h"
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QSplitter>
#include "QCommanderWidget.h"

struct PersistentOptions
{
    bool enabled = true;
    //bool autoReturn = true;

    const char * dataTag()
    {
        return "LuaCommanderOptions";
    }

    void dump()
    {
        std::cout << "LuaCommander:     enabled=" << enabled << std::endl;
        //std::cout << "LuaCommander:     autoReturn=" << autoReturn << std::endl;
    }

    bool load()
    {
#ifdef DEBUG_PERSISTENT_OPTIONS
        std::cout << "LuaCommander: Loading persistent options..." << std::endl;
#endif
        simInt dataLength;
        simChar *pdata = simPersistentDataRead(dataTag(), &dataLength);
        if(!pdata)
        {
#ifdef DEBUG_PERSISTENT_OPTIONS
            std::cout << "LuaCommander: Could not load persistent options: null pointer error" << std::endl;
#endif
            return false;
        }
        bool ok = dataLength == sizeof(*this);
        if(ok)
        {
            memcpy(this, pdata, sizeof(*this));
            //UIFunctions::getInstance()->autoReturn.store(autoReturn);
#ifdef DEBUG_PERSISTENT_OPTIONS
            std::cout << "LuaCommander: Loaded persistent options:" << std::endl;
            dump();
#endif
        }
        else
        {
#ifdef DEBUG_PERSISTENT_OPTIONS
            std::cout << "LuaCommander: Could not load persistent options: incorrect data length " << dataLength << ", should be " << sizeof(*this) << std::endl;
#endif
        }
        simReleaseBuffer(pdata);
        return ok;
    }

    bool save()
    {
#ifdef DEBUG_PERSISTENT_OPTIONS
        std::cout << "LuaCommander: Saving persistent options:" << std::endl;
        dump();
#endif
        return simPersistentDataWrite(dataTag(), (simChar*)this, sizeof(*this), 1) != -1;
    }
};

class Plugin : public vrep::Plugin
{
public:
    void onStart()
    {
        UIProxy::getInstance(); // construct UIProxy here (UI thread)

        // find the StatusBar widget (QPlainTextEdit)
        QPlainTextEdit *statusBar = findStatusBar();
        if(!statusBar)
            throw std::runtime_error("cannot find the statusbar widget");

        if(!registerScriptStuff())
            throw std::runtime_error("failed to register script stuff");

        optionsChangedFromGui.store(false);
        optionsChangedFromData.store(false);

        // attach widget to V-REP main window
        splitter = (QSplitter*)statusBar->parentWidget();
        splitterChild = new QWidget();
        splitter->addWidget(splitterChild);
        QVBoxLayout *layout = new QVBoxLayout();
        layout->setSpacing(0);
        layout->setMargin(0);
        splitterChild->setLayout(layout);
        commanderWidget = new QCommanderWidget();
        commanderWidget->setVisible(false);
        layout->addWidget(statusBar);
        layout->addWidget(commanderWidget);
        splitterChild->setMaximumHeight(600);

        // add menu items to V-REP main window
        MENUITEM_TOGGLE_VISIBILITY = menuLabels.size();
        menuLabels.push_back("Enable");
        //MENUITEM_AUTO_RETURN = menuLabels.size();
        //menuLabels.push_back("Automatically return input statement");
        menuState.resize(menuLabels.size());
        menuHandles.resize(menuLabels.size());
        if(simAddModuleMenuEntry("Lua Commander", menuHandles.size(), &menuHandles[0]) == -1)
        {
            simAddStatusbarMessage("LuaCommander error: failed to create menu");
            return;
        }
        updateUI();
    }

    void onEnd()
    {
        if(!commanderWidget) return;

        // XXX: different platforms crash in some conditions
        //      this seems to make every platform happy:
#ifdef __APPLE__
        delete commanderWidget;
#else
        QPlainTextEdit *statusBar = findStatusBar();
        if(statusBar)
            splitter->addWidget(statusBar);
        delete splitterChild;
#endif
    }

    QPlainTextEdit * findStatusBar()
    {
        QPlainTextEdit *statusBar = UIProxy::vrepMainWindow->findChild<QPlainTextEdit*>("statusBar");
        return statusBar;
    }

    void updateMenuItems()
    {
        menuState[MENUITEM_TOGGLE_VISIBILITY] = (options.enabled ? itemChecked : 0) + itemEnabled;
        //menuState[MENUITEM_AUTO_RETURN] = (options.enabled ? itemEnabled : 0) + (options.autoReturn ? itemChecked : 0);
        for(int i = 0; i < menuHandles.size(); i++)
            simSetModuleMenuItemState(menuHandles[i], menuState[i], menuLabels[i].c_str());
    }

    void updateUI()
    {
        updateMenuItems();
        if(commanderWidget)
        {
            bool oldVis = commanderWidget->isVisible();

            if(!firstInstancePass)
                commanderWidget->setVisible(options.enabled);

            bool newVis = commanderWidget->isVisible();
            if(oldVis && !newVis)
            {
                // when commander is hidden, focus the statusbar
                QPlainTextEdit *statusBar = findStatusBar();
                if(statusBar)
                    statusBar->setFocus();
            }
        }
    }

    virtual void onMenuItemSelected(int itemHandle, int itemState)
    {
        if(itemHandle == menuHandles[MENUITEM_TOGGLE_VISIBILITY])
        {
            const int i = MENUITEM_TOGGLE_VISIBILITY;
            options.enabled = !options.enabled;
            optionsChangedFromGui.store(true);
            updateUI();
        }
        /*else if(itemHandle == menuHandles[MENUITEM_AUTO_RETURN])
        {
            const int i = MENUITEM_AUTO_RETURN;
            options.autoReturn = !options.autoReturn;
            UIFunctions::getInstance()->autoReturn = options.autoReturn;
            optionsChangedFromGui.store(true);
            updateUI();
        }*/
    }

    virtual void onGuiPass()
    {
        if(optionsChangedFromData.load())
        {
            optionsChangedFromData.store(false);
            updateUI();
        }
    }

    void updateScriptsList()
    {
        bool simRunning = simGetSimulationState() == sim_simulation_advancing_running;
        QMap<int,QString> childScripts;
        QMap<int,QString> customizationScripts;
        int i = 0;
        while(1)
        {
            int handle = simGetObjects(i++, sim_handle_all);
            if(handle == -1) break;
            char *name_cstr = simGetObjectName(handle);
            QString name = QString::fromUtf8(name_cstr);
            simReleaseBuffer(name_cstr);
            if(simRunning)
            {
                int childScript = simGetScriptAssociatedWithObject(handle);
                if(childScript != -1)
                    childScripts[handle] = name;
            }
            int customizationScript = simGetCustomizationScriptAssociatedWithObject(handle);
            if(customizationScript != -1)
                customizationScripts[handle] = name;
        }
        UIFunctions::getInstance()->scriptListChanged(childScripts, customizationScripts, simRunning);
    }

    virtual void onInstancePass(bool objectsErased, bool objectsCreated, bool modelLoaded, bool sceneLoaded, bool undoCalled, bool redoCalled, bool sceneSwitched, bool editModeActive, bool objectsScaled, bool selectionStateChanged, bool keyPressed, bool simulationStarted, bool simulationEnded, bool scriptCreated, bool scriptErased)
    {
        if(!commanderWidget) return;

        if(firstInstancePass)
        {
            firstInstancePass = false;

            int id = qRegisterMetaType< QMap<int,QString> >();

            UIFunctions::getInstance(); // construct UIFunctions here (SIM thread)
            QObject::connect(commanderWidget, &QCommanderWidget::execCode, UIFunctions::getInstance(), &UIFunctions::onExecCode);
            QObject::connect(UIFunctions::getInstance(), &UIFunctions::scriptListChanged, commanderWidget, &QCommanderWidget::onScriptListChanged);
            QCommanderEditor *editor = commanderWidget->editorWidget();
            QObject::connect(editor, &QCommanderEditor::getPrevCompletion, UIFunctions::getInstance(), &UIFunctions::onGetPrevCompletion);
            QObject::connect(editor, &QCommanderEditor::getNextCompletion, UIFunctions::getInstance(), &UIFunctions::onGetNextCompletion);
            QObject::connect(UIFunctions::getInstance(), &UIFunctions::setCompletion, editor, &QCommanderEditor::setCompletion);
            QObject::connect(editor, &QCommanderEditor::askCallTip, UIFunctions::getInstance(), &UIFunctions::onAskCallTip);
            QObject::connect(UIFunctions::getInstance(), &UIFunctions::setCallTip, editor, &QCommanderEditor::setCallTip);
            options.load();
            optionsChangedFromData.store(true);
        }

        if(commanderWidget->closeFlag.load())
        {
            commanderWidget->closeFlag.store(false);
            options.enabled = false;
            optionsChangedFromGui.store(true);
            updateUI();
        }

        if(optionsChangedFromGui.load())
        {
            optionsChangedFromGui.store(false);
            options.save();
            updateUI();
        }

        if(objectsErased || objectsCreated || modelLoaded || sceneLoaded || undoCalled || redoCalled || sceneSwitched || scriptCreated || scriptErased || simulationStarted || simulationEnded)
        {
            updateScriptsList();
        }
    }

private:
    bool firstInstancePass = true;
    bool pluginEnabled = true;
    QSplitter *splitter = 0L;
    QWidget *splitterChild = 0L;
    QCommanderWidget *commanderWidget = 0L;
    std::vector<simInt> menuHandles;
    std::vector<simInt> menuState;
    std::vector<std::string> menuLabels;
    int MENUITEM_TOGGLE_VISIBILITY;
    //int MENUITEM_AUTO_RETURN;
    static const int itemEnabled = 1, itemChecked = 2;
    std::atomic<bool> optionsChangedFromGui;
    std::atomic<bool> optionsChangedFromData;
    PersistentOptions options;
};

VREP_PLUGIN(PLUGIN_NAME, PLUGIN_VERSION, Plugin)
