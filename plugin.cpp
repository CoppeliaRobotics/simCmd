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

#include <string>
#include <vector>
#include <map>
#include <stdexcept>
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

class Plugin : public vrep::Plugin
{
public:
    void onStart()
    {
        if(!registerScriptStuff())
            throw std::runtime_error("failed to register script stuff");

        UIProxy::getInstance(); // construct UIProxy here (UI thread)

        // find the StatusBar widget (QPlainTextEdit)
        QList<QWidget*> children = UIProxy::vrepMainWindow->findChildren<QWidget*>();
        QPlainTextEdit *statusBar = 0L;
        foreach(QWidget *w, children)
        {
            if(QPlainTextEdit *qpe = qobject_cast<QPlainTextEdit*>(w))
            {
                statusBar = qpe;
                break;
            }
        }

        if(statusBar)
        {
            QWidget *parent = statusBar->parentWidget();
            commanderWidget = new QCommanderWidget(parent);
            QVBoxLayout *layout = new QVBoxLayout(parent);
            layout->setSpacing(0);
            layout->setMargin(0);
            parent->setLayout(layout);
            layout->addWidget(statusBar);
            layout->addWidget(commanderWidget);
        }
        else
        {
            simAddStatusbarMessage("LuaCommander: cannot find the statusbar widget");
        }
    }

    virtual void onInstancePass(bool objectsErased, bool objectsCreated, bool modelLoaded, bool sceneLoaded, bool undoCalled, bool redoCalled, bool sceneSwitched, bool editModeActive, bool objectsScaled, bool selectionStateChanged, bool keyPressed, bool simulationStarted, bool simulationEnded)
    {
        int id = qRegisterMetaType< QMap<int,QString> >();

        if(firstInstancePass)
        {
            firstInstancePass = false;
            UIFunctions::getInstance(); // construct UIFunctions here (SIM thread)
            QObject::connect(commanderWidget, &QCommanderWidget::execCode, UIFunctions::getInstance(), &UIFunctions::onExecCode);
            QObject::connect(UIFunctions::getInstance(), &UIFunctions::scriptListChanged, commanderWidget, &QCommanderWidget::onScriptListChanged);
        }

        if(objectsErased || objectsCreated || modelLoaded || sceneLoaded || undoCalled || redoCalled || sceneSwitched)
        {
            DBG << "object list changed" << std::endl;
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
                int childScript = simGetScriptAssociatedWithObject(handle);
                int customizationScript = simGetCustomizationScriptAssociatedWithObject(handle);
                // TODO: add joint ctrl callbacks
                if(childScript != -1)
                    childScripts[handle] = name;
                if(customizationScript != -1)
                    customizationScripts[handle] = name;
            }
            UIFunctions::getInstance()->scriptListChanged(childScripts, jointCtrlCalbacks, customizationScripts);
        }
    }

private:
    bool firstInstancePass = true;
    QCommanderWidget *commanderWidget = 0L;
};

VREP_PLUGIN(PLUGIN_NAME, PLUGIN_VERSION, Plugin)
