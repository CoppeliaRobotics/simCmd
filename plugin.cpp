// Copyright 2016 Coppelia Robotics GmbH. All rights reserved.
// marc@coppeliarobotics.com
// www.coppeliarobotics.com
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// -------------------------------------------------------------------
// Authors:
// Federico Ferri <federico.ferri.it at gmail dot com>
// -------------------------------------------------------------------

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
#include "QCommanderWidget.h"

struct PersistentOptions
{
    bool enabled = true;
    bool autoReturn = true;

    const char * dataTag()
    {
        return "LuaCommanderOptions";
    }

    bool load()
    {
        std::cout << "Loading persistent options..." << std::endl;
        simInt dataLength;
        simChar *pdata = simPersistentDataRead(dataTag(), &dataLength);
        if(!pdata)
        {
            std::cout << "Could not load persistent options: null pointer error" << std::endl;
            return false;
        }
        bool ok = dataLength == sizeof(*this);
        if(ok)
        {
            memcpy(pdata, this, sizeof(*this));
            UIFunctions::getInstance()->autoReturn.store(autoReturn);
            std::cout << "Loaded persistent options: enabled=" << enabled << ", autoReturn=" << autoReturn << std::endl;
        }
        else
        {
            std::cout << "Could not load persistent options: incorrect data length " << dataLength << ", should be " << sizeof(*this) << std::endl;
        }
        simReleaseBuffer(pdata);
        return ok;
    }

    bool save()
    {
        return simPersistentDataWrite(dataTag(), (simChar*)this, sizeof(*this), 1) != -1;
    }
};

class Plugin : public vrep::Plugin
{
public:
    void onStart()
    {
        if(!registerScriptStuff())
            throw std::runtime_error("failed to register script stuff");

        UIProxy::getInstance(); // construct UIProxy here (UI thread)

        // find the StatusBar widget (QPlainTextEdit)
        QPlainTextEdit *statusBar = findStatusBar();
        if(!statusBar)
        {
            simAddStatusbarMessage("LuaCommander error: cannot find the statusbar widget");
            return;
        }

        // attach widget to V-REP main window
        QWidget *parent = statusBar->parentWidget();
        commanderWidget = new QCommanderWidget(parent);
        QVBoxLayout *layout = new QVBoxLayout(parent);
        layout->setSpacing(0);
        layout->setMargin(0);
        parent->setLayout(layout);
        layout->addWidget(statusBar);
        layout->addWidget(commanderWidget);

        // add menu items to V-REP main window
        MENUITEM_TOGGLE_VISIBILITY = menuLabels.size();
        menuLabels.push_back("Enable");
        menuState.push_back((options.enabled ? itemChecked : 0) + itemEnabled);

        MENUITEM_AUTO_RETURN = menuLabels.size();
        menuLabels.push_back("Automatically return input statement");
        menuState.push_back((options.enabled ? itemEnabled : 0) + (options.autoReturn ? itemChecked : 0));

        menuHandles.resize(menuLabels.size());

        if(simAddModuleMenuEntry("Lua Commander", menuHandles.size(), &menuHandles[0]) == -1)
        {
            simAddStatusbarMessage("LuaCommander error: failed to create menu");
            return;
        }
        updateMenuItems();
    }

    QPlainTextEdit * findStatusBar()
    {
        QPlainTextEdit *statusBar = UIProxy::vrepMainWindow->findChild<QPlainTextEdit*>("statusBar");
        if(statusBar) return statusBar;

        // we have an old V-REP version, find the statusBar widget in an alternative way:
        QList<QWidget*> children = UIProxy::vrepMainWindow->findChildren<QWidget*>();
        foreach(QWidget *w, children)
        {
            if(QPlainTextEdit *qpe = qobject_cast<QPlainTextEdit*>(w))
            {
                return qpe;
            }
        }
        return 0L;
    }

    void updateMenuItems()
    {
        for(int i = 0; i < menuHandles.size(); i++)
            simSetModuleMenuItemState(menuHandles[i], menuState[i], menuLabels[i].c_str());
        if(commanderWidget)
            commanderWidget->setVisible(options.enabled);
    }

    virtual void onMenuItemSelected(int itemHandle, int itemState)
    {
        if(itemHandle == menuHandles[MENUITEM_TOGGLE_VISIBILITY])
        {
            const int i = MENUITEM_TOGGLE_VISIBILITY;
            options.enabled = !options.enabled;
            menuState[i] = (options.enabled ? itemChecked : 0) + itemEnabled;
            optionsChangedFromGui.store(true);
            updateMenuItems();
        }
        else if(itemHandle == menuHandles[MENUITEM_AUTO_RETURN])
        {
            const int i = MENUITEM_AUTO_RETURN;
            options.autoReturn = !options.autoReturn;
            UIFunctions::getInstance()->autoReturn = options.autoReturn;
            menuState[i] = (options.enabled ? itemEnabled : 0) + (options.autoReturn ? itemChecked : 0);
            optionsChangedFromGui.store(true);
            updateMenuItems();
        }
    }

    virtual void onGuiPass()
    {
        if(optionsChangedFromData.load())
        {
            optionsChangedFromData.store(false);
            updateMenuItems();
        }
    }

    void updateScriptsList()
    {
        bool simRunning = simGetSimulationState() == sim_simulation_advancing_running;
        QMap<int,QString> childScripts;
        QMap<int,QString> jointCtrlCalbacks;
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
            // TODO: add joint ctrl callbacks
        }
        UIFunctions::getInstance()->scriptListChanged(childScripts, jointCtrlCalbacks, customizationScripts);
    }

    virtual void onInstancePass(bool objectsErased, bool objectsCreated, bool modelLoaded, bool sceneLoaded, bool undoCalled, bool redoCalled, bool sceneSwitched, bool editModeActive, bool objectsScaled, bool selectionStateChanged, bool keyPressed, bool simulationStarted, bool simulationEnded, bool scriptCreated, bool scriptErased)
    {
        int id = qRegisterMetaType< QMap<int,QString> >();

        if(firstInstancePass)
        {
            firstInstancePass = false;
            UIFunctions::getInstance(); // construct UIFunctions here (SIM thread)
            QObject::connect(commanderWidget, &QCommanderWidget::execCode, UIFunctions::getInstance(), &UIFunctions::onExecCode);
            QObject::connect(UIFunctions::getInstance(), &UIFunctions::scriptListChanged, commanderWidget, &QCommanderWidget::onScriptListChanged);

            if(options.load())
                optionsChangedFromData.store(true);
        }

        if(optionsChangedFromGui.load())
        {
            optionsChangedFromGui.store(false);
            simPersistentDataWrite("LuaCommanderOptions", (simChar*)&options, sizeof(options), 1);
        }

        if(objectsErased || objectsCreated || modelLoaded || sceneLoaded || undoCalled || redoCalled || sceneSwitched || scriptCreated || scriptErased)
        {
            updateScriptsList();
        }
    }

    virtual void onSimulationAboutToStart()
    {
        updateScriptsList();
    }

    virtual void onSimulationEnded()
    {
        updateScriptsList();
    }

private:
    bool firstInstancePass = true;
    bool pluginEnabled = true;
    QCommanderWidget *commanderWidget = 0L;
    std::vector<simInt> menuHandles;
    std::vector<simInt> menuState;
    std::vector<std::string> menuLabels;
    int MENUITEM_TOGGLE_VISIBILITY;
    int MENUITEM_AUTO_RETURN;
    static const int itemEnabled = 1, itemChecked = 2;
    std::atomic<bool> optionsChangedFromGui;
    std::atomic<bool> optionsChangedFromData;
    PersistentOptions options;
};

VREP_PLUGIN(PLUGIN_NAME, PLUGIN_VERSION, Plugin)
