sim=require'sim'

options={}

menus={
    {
        label='',
        enabled=true,
    },
    {
        label='Clear command history',
        enabled=true,
        callback=function(self)
            simLuaCmd.clearHistory()
        end,
    },
    {
        label='',
        enabled=true,
    },
    --[[
    -- non-boolean options:
    --     "simLuaCmd.historySize" [int]
    --     "simLuaCmd.arrayMaxItemsDisplayed" [int]
    --     "simLuaCmd.stringLongLimit" [int]
    --     "simLuaCmd.floatPrecision" [int]
    --     "simLuaCmd.mapMaxDepth" [int]
    --]]
    {
        label='Print all returned values',
        enabled=true,
        checkable=true,
        checked=true,
        callback=function(self)
            sim.setNamedBoolParam('simLuaCmd.printAllReturnedValues',self.checked)
        end,
    },
    {
        label='Warn about multiple returned values',
        enabled=true,
        checkable=true,
        checked=true,
        callback=function(self)
            sim.setNamedBoolParam('simLuaCmd.warnAboutMultipleReturnedValues',self.checked)
        end,
    },
    {
        label='String rendering: escape special characters',
        enabled=true,
        checkable=true,
        checked=true,
        callback=function(self)
            sim.setNamedBoolParam('simLuaCmd.stringEscapeSpecials',self.checked)
        end,
    },
    {
        label='Map/array rendering: sort keys by name',
        enabled=true,
        checkable=true,
        checked=true,
        callback=function(self)
            sim.setNamedBoolParam('simLuaCmd.mapSortKeysByName',self.checked)
        end,
    },
    {
        label='Map/array rendering: sort keys by type',
        enabled=true,
        checkable=true,
        checked=true,
        callback=function(self)
            sim.setNamedBoolParam('simLuaCmd.mapSortKeysByType',self.checked)
        end,
    },
    {
        label='Map/array rendering: shadow long strings',
        enabled=true,
        checkable=true,
        checked=true,
        callback=function(self)
            sim.setNamedBoolParam('simLuaCmd.mapShadowLongStrings',self.checked)
        end,
    },
    {
        label='Map/array rendering: shadow buffer strings',
        enabled=true,
        checkable=true,
        checked=true,
        callback=function(self)
            sim.setNamedBoolParam('simLuaCmd.mapShadowBufferStrings',self.checked)
        end,
    },
    {
        label='Map/array rendering: shadow strings with special characters',
        enabled=true,
        checkable=true,
        checked=true,
        callback=function(self)
            sim.setNamedBoolParam('simLuaCmd.mapShadowSpecialStrings',self.checked)
        end,
    },
    {
        label='History: skip repeated commands',
        enabled=true,
        checkable=true,
        checked=true,
        callback=function(self)
            sim.setNamedBoolParam('simLuaCmd.historySkipRepeated',self.checked)
        end,
    },
    {
        label='History: remove duplicates',
        enabled=true,
        checkable=true,
        checked=true,
        callback=function(self)
            sim.setNamedBoolParam('simLuaCmd.historyRemoveDups',self.checked)
        end,
    },
    {
        label='History: show matching entries (select with UP)',
        enabled=true,
        checkable=true,
        checked=false,
        callback=function(self)
            sim.setNamedBoolParam('simLuaCmd.showMatchingHistory',self.checked)
        end,
    },
    {
        label='Set convenience vars',
        enabled=true,
        checkable=true,
        checked=true,
        callback=function(self)
            sim.setNamedBoolParam('simLuaCmd.setConvenienceVars',self.checked)
        end,
    },
    {
        label='Auto-accept common completion prefix',
        enabled=true,
        checkable=true,
        checked=true,
        callback=function(self)
            sim.setNamedBoolParam('simLuaCmd.autoAcceptCommonCompletionPrefix',self.checked)
        end,
    },
    {
        label='Resize statusbar when focused',
        enabled=true,
        checkable=true,
        checked=false,
        callback=function(self)
            sim.setNamedBoolParam('simLuaCmd.resizeStatusbarWhenFocused',self.checked)
        end,
    },
}

function menuItemState(menu)
    local state=0
    if menu.enabled then state=state+1 end
    if menu.checked then state=state+2 end
    if menu.checkable then state=state+4 end
    return state
end

function updateMenuItems()
    for i,menu in ipairs(menus) do
        if menu.handle then
            sim.moduleEntry(menu.handle,nil,menuItemState(menu))
        end
    end
end

function sysCall_info()
    return {menu='Developer tools\nLua Commander\n(addon)'}
end

function sysCall_init()
    simLuaCmd=require'simLuaCmd'
    for i,menu in ipairs(menus) do
        menu.handle=sim.moduleEntry(-1,'Developer tools\nLua Commander\n'..menu.label,menuItemState(menu))
    end
end

function sysCall_moduleEntry(inData)
    for i,menu in ipairs(menus) do
        if menu.handle==inData.handle then
            if menu.checkable then
                menu.checked=not menu.checked
            end
            if menu.callback then
                menu.callback(menu)
            end
        end
    end
    updateMenuItems()
end

function sysCall_addOnScriptSuspend()
    return {cmd='cleanup'}
end

function sysCall_cleanup()
    unloadPlugin(simLuaCmd.pluginHandle)
end
