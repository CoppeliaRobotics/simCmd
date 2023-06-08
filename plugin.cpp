#include <atomic>
#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>
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
#include "PersistentOptions.h"
#include "qluacommanderwidget.h"

class Plugin : public sim::Plugin
{
public:
    void onInit() override
    {
        if(sim::getBoolParam(sim_boolparam_headless))
            throw std::runtime_error("doesn't work in headless mode");

        if(!registerScriptStuff())
            throw std::runtime_error("failed to register script stuff");

        setExtVersion("Lua REPL (read-eval-print-loop) Plugin");
        setBuildDate(BUILD_DATE);

        SIM::getInstance(); // construct SIM here (SIM thread)

        optionsChangedFromGui.store(false);
        optionsChangedFromData.store(false);

        addMenuItems();
    }

    void onCleanup() override
    {
        SIM::destroyInstance();
        SIM_THREAD = NULL;
    }

    void onUIInit() override
    {
        firstInstancePass = true;

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
        commanderWidget->setVisible(false);
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

    int addMenuItem(const std::string &label, int flags)
    {
        return sim::moduleEntry("Developer tools\nLua Commander\n" + label, flags);
    }

    void addMenuItems()
    {
        MENUITEM_TOGGLE_VISIBILITY = addMenuItem("Enable", itemEnabled|itemCheckable);
        addMenuItem("", itemEnabled);
        MENUITEM_HISTORY_CLEAR = addMenuItem("Clear command history", itemEnabled);
        addMenuItem("", itemEnabled);
        MENUITEM_PRINT_ALL_RETURNED_VALUES = addMenuItem("Print all returned values", itemEnabled|itemCheckable);
        MENUITEM_WARN_ABOUT_MULTIPLE_RETURNED_VALUES = addMenuItem("Warn about multiple returned values", itemEnabled|itemCheckable);
        MENUITEM_STRING_ESCAPE_SPECIALS = addMenuItem("String rendering: escape special characters", itemEnabled|itemCheckable);
        MENUITEM_MAP_SORT_KEYS_BY_NAME = addMenuItem("Map/array rendering: sort keys by name", itemEnabled|itemCheckable);
        MENUITEM_MAP_SORT_KEYS_BY_TYPE = addMenuItem("Map/array rendering: sort keys by type", itemEnabled|itemCheckable);
        MENUITEM_MAP_SHADOW_LONG_STRINGS = addMenuItem("Map/array rendering: shadow long strings", itemEnabled|itemCheckable);
        MENUITEM_MAP_SHADOW_BUFFER_STRINGS = addMenuItem("Map/array rendering: shadow buffer strings", itemEnabled|itemCheckable);
        MENUITEM_MAP_SHADOW_SPECIAL_STRINGS = addMenuItem("Map/array rendering: shadow strings with special characters", itemEnabled|itemCheckable);
        MENUITEM_HISTORY_SKIP_REPEATED = addMenuItem("History: skip repeated commands", itemEnabled|itemCheckable);
        MENUITEM_HISTORY_REMOVE_DUPS = addMenuItem("History: remove duplicates", itemEnabled|itemCheckable);
        MENUITEM_SHOW_MATCHING_HISTORY = addMenuItem("History: show matching entries (select with UP)", itemEnabled|itemCheckable);
        MENUITEM_DYNAMIC_COMPLETION = addMenuItem("Dynamic completion", itemEnabled|itemCheckable);
        MENUITEM_AUTO_ACCEPT_COMMON_COMPLETION_PREFIX = addMenuItem("Auto-accept common completion prefix", itemEnabled|itemCheckable);
        MENUITEM_RESIZE_STATUSBAR_WHEN_FOCUSED = addMenuItem("Resize statusbar when focused", itemEnabled|itemCheckable);
    }

    void updateMenuItems()
    {
        sim::moduleEntry(MENUITEM_TOGGLE_VISIBILITY, (options.enabled ? itemChecked : 0) + itemEnabled);
        sim::moduleEntry(MENUITEM_HISTORY_CLEAR, (options.enabled ? itemEnabled : 0));
        sim::moduleEntry(MENUITEM_PRINT_ALL_RETURNED_VALUES, (options.enabled ? itemEnabled : 0) + (options.printAllReturnedValues ? itemChecked : 0));
        sim::moduleEntry(MENUITEM_WARN_ABOUT_MULTIPLE_RETURNED_VALUES, (options.enabled ? itemEnabled : 0) + (options.warnAboutMultipleReturnedValues ? itemChecked : 0));
        sim::moduleEntry(MENUITEM_STRING_ESCAPE_SPECIALS, (options.enabled ? itemEnabled : 0) + (options.stringEscapeSpecials ? itemChecked : 0));
        sim::moduleEntry(MENUITEM_MAP_SORT_KEYS_BY_NAME, (options.enabled ? itemEnabled : 0) + (options.mapSortKeysByName ? itemChecked : 0));
        sim::moduleEntry(MENUITEM_MAP_SORT_KEYS_BY_TYPE, (options.enabled ? itemEnabled : 0) + (options.mapSortKeysByType ? itemChecked : 0));
        sim::moduleEntry(MENUITEM_MAP_SHADOW_LONG_STRINGS, (options.enabled ? itemEnabled : 0) + (options.mapShadowLongStrings ? itemChecked : 0));
        sim::moduleEntry(MENUITEM_MAP_SHADOW_BUFFER_STRINGS, (options.enabled ? itemEnabled : 0) + (options.mapShadowBufferStrings ? itemChecked : 0));
        sim::moduleEntry(MENUITEM_MAP_SHADOW_SPECIAL_STRINGS, (options.enabled ? itemEnabled : 0) + (options.mapShadowSpecialStrings ? itemChecked : 0));
        sim::moduleEntry(MENUITEM_HISTORY_SKIP_REPEATED, (options.enabled ? itemEnabled : 0) + (options.historySkipRepeated ? itemChecked : 0));
        sim::moduleEntry(MENUITEM_HISTORY_REMOVE_DUPS, (options.enabled ? itemEnabled : 0) + (options.historyRemoveDups ? itemChecked : 0));
        sim::moduleEntry(MENUITEM_SHOW_MATCHING_HISTORY, (options.enabled ? itemEnabled : 0) + (options.showMatchingHistory ? itemChecked : 0));
        sim::moduleEntry(MENUITEM_DYNAMIC_COMPLETION, (options.enabled ? itemEnabled : 0) + (options.autoAcceptCommonCompletionPrefix ? itemChecked : 0));
        sim::moduleEntry(MENUITEM_AUTO_ACCEPT_COMMON_COMPLETION_PREFIX, (options.enabled ? itemEnabled : 0) + (options.dynamicCompletion ? itemChecked : 0));
        sim::moduleEntry(MENUITEM_RESIZE_STATUSBAR_WHEN_FOCUSED, (options.enabled ? itemEnabled : 0) + (options.resizeStatusbarWhenFocused ? itemChecked : 0));
    }

    void updateUI()
    {
        if(!commanderWidget) return;

        bool oldVis = commanderWidget->isVisible();

        if(!firstInstancePass)
            commanderWidget->setVisible(options.enabled);

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

    void onUIMenuItemSelected(int itemHandle, int itemState) override
    {
        if(itemHandle == MENUITEM_TOGGLE_VISIBILITY)
        {
            options.enabled = !options.enabled;
        }
        else if(itemHandle == MENUITEM_HISTORY_CLEAR)
        {
            UI::getInstance()->clearHistory();
        }
        else if(itemHandle == MENUITEM_PRINT_ALL_RETURNED_VALUES)
        {
            options.printAllReturnedValues = !options.printAllReturnedValues;
        }
        else if(itemHandle == MENUITEM_WARN_ABOUT_MULTIPLE_RETURNED_VALUES)
        {
            options.warnAboutMultipleReturnedValues = !options.warnAboutMultipleReturnedValues;
        }
        else if(itemHandle == MENUITEM_STRING_ESCAPE_SPECIALS)
        {
            options.stringEscapeSpecials = !options.stringEscapeSpecials;
        }
        else if(itemHandle == MENUITEM_MAP_SORT_KEYS_BY_NAME)
        {
            options.mapSortKeysByName = !options.mapSortKeysByName;
        }
        else if(itemHandle == MENUITEM_MAP_SORT_KEYS_BY_TYPE)
        {
            options.mapSortKeysByType = !options.mapSortKeysByType;
        }
        else if(itemHandle == MENUITEM_MAP_SHADOW_LONG_STRINGS)
        {
            options.mapShadowLongStrings = !options.mapShadowLongStrings;
        }
        else if(itemHandle == MENUITEM_MAP_SHADOW_BUFFER_STRINGS)
        {
            options.mapShadowBufferStrings = !options.mapShadowBufferStrings;
        }
        else if(itemHandle == MENUITEM_MAP_SHADOW_SPECIAL_STRINGS)
        {
            options.mapShadowSpecialStrings = !options.mapShadowSpecialStrings;
        }
        else if(itemHandle == MENUITEM_HISTORY_SKIP_REPEATED)
        {
            options.historySkipRepeated = !options.historySkipRepeated;
        }
        else if(itemHandle == MENUITEM_HISTORY_REMOVE_DUPS)
        {
            options.historyRemoveDups = !options.historyRemoveDups;
        }
        else if(itemHandle == MENUITEM_SHOW_MATCHING_HISTORY)
        {
            options.showMatchingHistory = !options.showMatchingHistory;
        }
        else if(itemHandle == MENUITEM_DYNAMIC_COMPLETION)
        {
            options.dynamicCompletion = !options.dynamicCompletion;
        }
        else if(itemHandle == MENUITEM_AUTO_ACCEPT_COMMON_COMPLETION_PREFIX)
        {
            options.autoAcceptCommonCompletionPrefix = !options.autoAcceptCommonCompletionPrefix;
        }
        else if(itemHandle == MENUITEM_RESIZE_STATUSBAR_WHEN_FOCUSED)
        {
            options.resizeStatusbarWhenFocused = !options.resizeStatusbarWhenFocused;
        }
        else return;

        optionsChangedFromGui.store(true);
        updateUI();
        commanderWidget->setOptions(options);
    }

    void updateScriptsList()
    {
        bool isRunning = sim::getSimulationState() == sim_simulation_advancing_running;
        QMap<int, QString> childScripts;
        QMap<int, QString> customizationScripts;
        for(int handle : sim::getObjects(sim_handle_all))
        {
            QString name = QString::fromStdString(sim::getObjectAlias(handle, 5));
            if(isRunning)
            {
                int childScript = sim::getScriptHandleEx(sim_scripttype_childscript, handle);
                if(childScript != -1)
                    childScripts[handle] = name;
            }
            int customizationScript = sim::getScriptHandleEx(sim_scripttype_customizationscript, handle);
            if(customizationScript != -1)
                customizationScripts[handle] = name;
        }
        SIM::getInstance()->scriptListChanged(childScripts, customizationScripts, isRunning);
    }

    void onUIPass() override
    {
        if(optionsChangedFromData.load())
        {
            optionsChangedFromData.store(false);
            updateUI();
            commanderWidget->setOptions(options);
        }
    }

    void onInstancePass(const sim::InstancePassFlags &flags) override
    {
        if(!commanderWidget) return;

        if(firstInstancePass)
        {
            int id = qRegisterMetaType< QMap<int,QString> >();

            SIM *sim = SIM::getInstance();
            QObject::connect(commanderWidget, &QLuaCommanderWidget::execCode, sim, &SIM::onExecCode);
            QObject::connect(commanderWidget, &QLuaCommanderWidget::askCompletion, sim, &SIM::onAskCompletion);
            QObject::connect(sim, &SIM::scriptListChanged, commanderWidget, &QLuaCommanderWidget::onScriptListChanged);
            QObject::connect(sim, &SIM::setCompletion, commanderWidget, &QLuaCommanderWidget::onSetCompletion);
            QObject::connect(commanderWidget, &QLuaCommanderWidget::askCallTip, sim, &SIM::onAskCallTip);
            QObject::connect(sim, &SIM::setCallTip, commanderWidget, &QLuaCommanderWidget::onSetCallTip);
            QObject::connect(sim, &SIM::historyChanged, commanderWidget, &QLuaCommanderWidget::setHistory);
            options.load();
            sim->setOptions(options);
            sim->loadHistory();
            optionsChangedFromData.store(true);
        }

        updateMenuItems();

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
            SIM::getInstance()->setOptions(options);
            updateUI();
        }

        if(firstInstancePass || flags.objectsErased || flags.objectsCreated || flags.modelLoaded || flags.sceneLoaded || flags.undoCalled || flags.redoCalled || flags.sceneSwitched || flags.scriptCreated || flags.scriptErased || flags.simulationStarted || flags.simulationEnded)
        {
            updateScriptsList();
        }

        firstInstancePass = false;
    }

    void setEnabled(setEnabled_in *in, setEnabled_out *out)
    {
        options.enabled = in->b;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setPrintAllReturnedValues(setPrintAllReturnedValues_in *in, setPrintAllReturnedValues_out *out)
    {
        options.printAllReturnedValues = in->b;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setWarnAboutMultipleReturnedValues(setWarnAboutMultipleReturnedValues_in *in, setWarnAboutMultipleReturnedValues_out *out)
    {
        options.warnAboutMultipleReturnedValues = in->b;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setArrayMaxItemsDisplayed(setArrayMaxItemsDisplayed_in *in, setArrayMaxItemsDisplayed_out *out)
    {
        options.arrayMaxItemsDisplayed = in->n;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setStringLongLimit(setStringLongLimit_in *in, setStringLongLimit_out *out)
    {
        options.stringLongLimit = in->n;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setStringEscapeSpecials(setStringEscapeSpecials_in *in, setStringEscapeSpecials_out *out)
    {
        options.stringEscapeSpecials = in->b;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setMapSortKeysByName(setMapSortKeysByName_in *in, setMapSortKeysByName_out *out)
    {
        options.mapSortKeysByName = in->b;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setMapSortKeysByType(setMapSortKeysByType_in *in, setMapSortKeysByType_out *out)
    {
        options.mapSortKeysByType = in->b;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setMapShadowLongStrings(setMapShadowLongStrings_in *in, setMapShadowLongStrings_out *out)
    {
        options.mapShadowLongStrings = in->b;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setMapShadowBufferStrings(setMapShadowBufferStrings_in *in, setMapShadowBufferStrings_out *out)
    {
        options.mapShadowBufferStrings = in->b;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setMapShadowSpecialStrings(setMapShadowSpecialStrings_in *in, setMapShadowSpecialStrings_out *out)
    {
        options.mapShadowSpecialStrings = in->b;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setFloatPrecision(setFloatPrecision_in *in, setFloatPrecision_out *out)
    {
        options.floatPrecision = in->n;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setMapMaxDepth(setMapMaxDepth_in *in, setMapMaxDepth_out *out)
    {
        options.mapMaxDepth = in->n;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void clearHistory(clearHistory_in *in, clearHistory_out *out)
    {
        SIM::getInstance()->clearHistory();
    }

    void setHistorySize(setHistorySize_in *in, setHistorySize_out *out)
    {
        options.historySize = in->n;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setHistorySkipRepeated(setHistorySkipRepeated_in *in, setHistorySkipRepeated_out *out)
    {
        options.historySkipRepeated = in->b;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setHistoryRemoveDups(setHistoryRemoveDups_in *in, setHistoryRemoveDups_out *out)
    {
        options.historyRemoveDups = in->b;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setShowMatchingHistory(setShowMatchingHistory_in *in, setShowMatchingHistory_out *out)
    {
        options.showMatchingHistory = in->b;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setDynamicCompletion(setDynamicCompletion_in *in, setDynamicCompletion_out *out)
    {
        options.dynamicCompletion = in->b;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setAutoAcceptCommonCompletionPrefix(setAutoAcceptCommonCompletionPrefix_in *in, setAutoAcceptCommonCompletionPrefix_out *out)
    {
        options.autoAcceptCommonCompletionPrefix = in->b;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

    void setResizeStatusbarWhenFocused(setResizeStatusbarWhenFocused_in *in, setResizeStatusbarWhenFocused_out *out)
    {
        options.resizeStatusbarWhenFocused = in->b;
        options.save();
        SIM::getInstance()->setOptions(options);
        optionsChangedFromData.store(true);
    }

private:
    std::atomic<bool> optionsChangedFromGui;
    std::atomic<bool> optionsChangedFromData;
    PersistentOptions options;
    bool firstInstancePass = true;
    QPlainTextEdit *statusBar;
    QSplitter *splitter = 0L;
    QWidget *splitterChild = 0L;
    QVBoxLayout *layout = 0L;
    QLuaCommanderWidget *commanderWidget = 0L;
    int MENUITEM_TOGGLE_VISIBILITY;
    int MENUITEM_HISTORY_CLEAR;
    int MENUITEM_PRINT_ALL_RETURNED_VALUES;
    int MENUITEM_WARN_ABOUT_MULTIPLE_RETURNED_VALUES;
    int MENUITEM_STRING_ESCAPE_SPECIALS;
    int MENUITEM_MAP_SORT_KEYS_BY_NAME;
    int MENUITEM_MAP_SORT_KEYS_BY_TYPE;
    int MENUITEM_MAP_SHADOW_LONG_STRINGS;
    int MENUITEM_MAP_SHADOW_BUFFER_STRINGS;
    int MENUITEM_MAP_SHADOW_SPECIAL_STRINGS;
    int MENUITEM_HISTORY_SKIP_REPEATED;
    int MENUITEM_HISTORY_REMOVE_DUPS;
    int MENUITEM_SHOW_MATCHING_HISTORY;
    int MENUITEM_DYNAMIC_COMPLETION;
    int MENUITEM_AUTO_ACCEPT_COMMON_COMPLETION_PREFIX;
    int MENUITEM_RESIZE_STATUSBAR_WHEN_FOCUSED;
    static const int itemEnabled = 1, itemChecked = 2, itemCheckable = 4;
};

SIM_UI_PLUGIN(PLUGIN_NAME, PLUGIN_VERSION, Plugin)
#include "stubsPlusPlus.cpp"
