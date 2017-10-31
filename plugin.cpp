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
            QCommanderWidget *commanderWidget = new QCommanderWidget(parent);
            commanderWidget->setPlaceholderText("Input Lua code here");
            commanderWidget->setFont(QFont("Courier", 12));
            QVBoxLayout *layout = new QVBoxLayout(parent);
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
        if(firstInstancePass)
        {
            firstInstancePass = false;
            UIFunctions::getInstance(); // construct UIFunctions here (SIM thread)
        }
    }

private:
    bool firstInstancePass = true;
};

VREP_PLUGIN(PLUGIN_NAME, PLUGIN_VERSION, Plugin)
