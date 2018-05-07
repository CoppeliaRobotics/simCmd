#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>
#include <boost/format.hpp>
#include <boost/foreach.hpp>
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
    int arrayMaxItemsDisplayed = 20;
    int stringLongLimit = 160;
    bool mapSortKeysByName = true;
    bool mapSortKeysByType = true;
    bool mapShadowLongStrings = true;
    bool mapShadowBufferStrings = true;
    bool mapShadowSpecialStrings = true;

    const char * dataTag()
    {
        return "LuaCommanderOptions";
    }

    void dump()
    {
        std::cout << "LuaCommander:     enabled=" << enabled << std::endl;
        std::cout << "LuaCommander:     arrayMaxItemsDisplayed=" << arrayMaxItemsDisplayed << std::endl;
        std::cout << "LuaCommander:     stringLongLimit=" << stringLongLimit << std::endl;
        std::cout << "LuaCommander:     mapShadowBufferStrings=" << mapShadowBufferStrings << std::endl;
        std::cout << "LuaCommander:     mapShadowSpecialStrings=" << mapShadowSpecialStrings << std::endl;
        std::cout << "LuaCommander:     mapShadowLongStrings=" << mapShadowLongStrings << std::endl;
        std::cout << "LuaCommander:     mapSortKeysByName=" << mapSortKeysByName << std::endl;
        std::cout << "LuaCommander:     mapSortKeysByType=" << mapSortKeysByType << std::endl;
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
            UIFunctions::getInstance()->onSetArrayMaxItemsDisplayed(arrayMaxItemsDisplayed);
            UIFunctions::getInstance()->onSetStringLongLimit(stringLongLimit);
            UIFunctions::getInstance()->onSetMapSortKeysByName(mapSortKeysByName);
            UIFunctions::getInstance()->onSetMapSortKeysByType(mapSortKeysByType);
            UIFunctions::getInstance()->onSetMapShadowLongStrings(mapShadowLongStrings);
            UIFunctions::getInstance()->onSetMapShadowBufferStrings(mapShadowBufferStrings);
            UIFunctions::getInstance()->onSetMapShadowSpecialStrings(mapShadowSpecialStrings);
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

std::atomic<bool> optionsChangedFromGui;
std::atomic<bool> optionsChangedFromData;
PersistentOptions options;

void setArrayMaxItemsDisplayed(SScriptCallBack *p, const char *cmd, setArrayMaxItemsDisplayed_in *in, setArrayMaxItemsDisplayed_out *out)
{
    UIFunctions::getInstance()->onSetArrayMaxItemsDisplayed(in->n);
    options.arrayMaxItemsDisplayed = in->n;
    options.save();
    optionsChangedFromData.store(true);
}

void setStringLongLimit(SScriptCallBack *p, const char *cmd, setStringLongLimit_in *in, setStringLongLimit_out *out)
{
    UIFunctions::getInstance()->onSetStringLongLimit(in->n);
    options.stringLongLimit = in->n;
    options.save();
    optionsChangedFromData.store(true);
}

void setMapSortKeysByName(SScriptCallBack *p, const char *cmd, setMapSortKeysByName_in *in, setMapSortKeysByName_out *out)
{
    UIFunctions::getInstance()->onSetMapSortKeysByName(in->b);
    options.mapSortKeysByName = in->b;
    options.save();
    optionsChangedFromData.store(true);
}

void setMapSortKeysByType(SScriptCallBack *p, const char *cmd, setMapSortKeysByType_in *in, setMapSortKeysByType_out *out)
{
    UIFunctions::getInstance()->onSetMapSortKeysByType(in->b);
    options.mapSortKeysByType = in->b;
    options.save();
    optionsChangedFromData.store(true);
}

void setMapShadowLongStrings(SScriptCallBack *p, const char *cmd, setMapShadowLongStrings_in *in, setMapShadowLongStrings_out *out)
{
    UIFunctions::getInstance()->onSetMapShadowLongStrings(in->b);
    options.mapShadowLongStrings = in->b;
    options.save();
    optionsChangedFromData.store(true);
}

void setMapShadowBufferStrings(SScriptCallBack *p, const char *cmd, setMapShadowBufferStrings_in *in, setMapShadowBufferStrings_out *out)
{
    UIFunctions::getInstance()->onSetMapShadowBufferStrings(in->b);
    options.mapShadowBufferStrings = in->b;
    options.save();
    optionsChangedFromData.store(true);
}

void setMapShadowSpecialStrings(SScriptCallBack *p, const char *cmd, setMapShadowSpecialStrings_in *in, setMapShadowSpecialStrings_out *out)
{
    UIFunctions::getInstance()->onSetMapShadowSpecialStrings(in->b);
    options.mapShadowSpecialStrings = in->b;
    options.save();
    optionsChangedFromData.store(true);
}

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
        MENUITEM_MAP_SORT_KEYS_BY_NAME = menuLabels.size();
        menuLabels.push_back("Map/array rendering: sort keys by name");
        MENUITEM_MAP_SORT_KEYS_BY_TYPE = menuLabels.size();
        menuLabels.push_back("Map/array rendering: sort keys by type");
        MENUITEM_MAP_SHADOW_LONG_STRINGS = menuLabels.size();
        menuLabels.push_back("Map/array rendering: shadow long strings");
        MENUITEM_MAP_SHADOW_BUFFER_STRINGS = menuLabels.size();
        menuLabels.push_back("Map/array rendering: shadow buffer strings");
        MENUITEM_MAP_SHADOW_SPECIAL_STRINGS = menuLabels.size();
        menuLabels.push_back("Map/array rendering: shadow strings with special characters");

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
        menuState[MENUITEM_MAP_SORT_KEYS_BY_NAME] = (options.enabled ? itemEnabled : 0) + (options.mapSortKeysByName ? itemChecked : 0);
        menuState[MENUITEM_MAP_SORT_KEYS_BY_TYPE] = (options.enabled ? itemEnabled : 0) + (options.mapSortKeysByType ? itemChecked : 0);
        menuState[MENUITEM_MAP_SHADOW_LONG_STRINGS] = (options.enabled ? itemEnabled : 0) + (options.mapShadowLongStrings ? itemChecked : 0);
        menuState[MENUITEM_MAP_SHADOW_BUFFER_STRINGS] = (options.enabled ? itemEnabled : 0) + (options.mapShadowBufferStrings ? itemChecked : 0);
        menuState[MENUITEM_MAP_SHADOW_SPECIAL_STRINGS] = (options.enabled ? itemEnabled : 0) + (options.mapShadowSpecialStrings ? itemChecked : 0);

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
            options.enabled = !options.enabled;
        }
        else if(itemHandle == menuHandles[MENUITEM_MAP_SORT_KEYS_BY_NAME])
        {
            options.mapSortKeysByName = !options.mapSortKeysByName;
            UIProxy::getInstance()->setMapSortKeysByName(options.mapSortKeysByName);
        }
        else if(itemHandle == menuHandles[MENUITEM_MAP_SORT_KEYS_BY_TYPE])
        {
            options.mapSortKeysByType = !options.mapSortKeysByType;
            UIProxy::getInstance()->setMapSortKeysByType(options.mapSortKeysByType);
        }
        else if(itemHandle == menuHandles[MENUITEM_MAP_SHADOW_LONG_STRINGS])
        {
            options.mapShadowLongStrings = !options.mapShadowLongStrings;
            UIProxy::getInstance()->setMapShadowLongStrings(options.mapShadowLongStrings);
        }
        else if(itemHandle == menuHandles[MENUITEM_MAP_SHADOW_BUFFER_STRINGS])
        {
            options.mapShadowBufferStrings = !options.mapShadowBufferStrings;
            UIProxy::getInstance()->setMapShadowBufferStrings(options.mapShadowBufferStrings);
        }
        else if(itemHandle == menuHandles[MENUITEM_MAP_SHADOW_SPECIAL_STRINGS])
        {
            options.mapShadowSpecialStrings = !options.mapShadowSpecialStrings;
            UIProxy::getInstance()->setMapShadowSpecialStrings(options.mapShadowSpecialStrings);
        }
        else return;
        optionsChangedFromGui.store(true);
        updateUI();
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

    virtual void onInstancePass(const vrep::InstancePassFlags &flags, bool first)
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
            QObject::connect(UIProxy::getInstance(), &UIProxy::setArrayMaxItemsDisplayed, UIFunctions::getInstance(), &UIFunctions::onSetArrayMaxItemsDisplayed);
            QObject::connect(UIProxy::getInstance(), &UIProxy::setStringLongLimit, UIFunctions::getInstance(), &UIFunctions::onSetStringLongLimit);
            QObject::connect(UIProxy::getInstance(), &UIProxy::setMapSortKeysByName, UIFunctions::getInstance(), &UIFunctions::onSetMapSortKeysByName);
            QObject::connect(UIProxy::getInstance(), &UIProxy::setMapSortKeysByType, UIFunctions::getInstance(), &UIFunctions::onSetMapSortKeysByType);
            QObject::connect(UIProxy::getInstance(), &UIProxy::setMapShadowLongStrings, UIFunctions::getInstance(), &UIFunctions::onSetMapShadowLongStrings);
            QObject::connect(UIProxy::getInstance(), &UIProxy::setMapShadowBufferStrings, UIFunctions::getInstance(), &UIFunctions::onSetMapShadowBufferStrings);
            QObject::connect(UIProxy::getInstance(), &UIProxy::setMapShadowSpecialStrings, UIFunctions::getInstance(), &UIFunctions::onSetMapShadowSpecialStrings);
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

        if(flags.objectsErased || flags.objectsCreated || flags.modelLoaded || flags.sceneLoaded || flags.undoCalled || flags.redoCalled || flags.sceneSwitched || flags.scriptCreated || flags.scriptErased || flags.simulationStarted || flags.simulationEnded)
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
    int MENUITEM_MAP_SORT_KEYS_BY_NAME;
    int MENUITEM_MAP_SORT_KEYS_BY_TYPE;
    int MENUITEM_MAP_SHADOW_LONG_STRINGS;
    int MENUITEM_MAP_SHADOW_BUFFER_STRINGS;
    int MENUITEM_MAP_SHADOW_SPECIAL_STRINGS;
    static const int itemEnabled = 1, itemChecked = 2;
};

VREP_PLUGIN(PLUGIN_NAME, PLUGIN_VERSION, Plugin)
