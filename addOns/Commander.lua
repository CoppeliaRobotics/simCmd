local sim = require 'sim'
local simCmd

options = {}

-- LuaFormatter off
menus = {
    {
        label = '',
        enabled = true,
    },
    {
        label = 'Clear command history',
        enabled = true,
        menuCallback = function(self)
            simCmd.clearHistory()
        end,
    },
    {
        label = '',
        enabled = true,
    },
    --[[
    -- non-boolean options:
    --     "simCmd.historySize" [int]
    --     "simCmd.arrayMaxItemsDisplayed" [int]
    --     "simCmd.stringLongLimit" [int]
    --     "simCmd.floatPrecision" [int]
    --     "simCmd.mapMaxDepth" [int]
    --]]
    --[[
    {
        label = 'Print all returned values',
        enabled = true,
        checkable = true,
        checked = true,
        propertyName = 'customData.simCmd.printAllReturnedValues',
    },
    {
        label = 'Warn about multiple returned values',
        enabled = true,
        checkable = true,
        checked = true,
        propertyName = 'customData.simCmd.warnAboutMultipleReturnedValues',
    },
    {
        label = 'String rendering: escape special characters',
        enabled = true,
        checkable = true,
        checked = true,
        propertyName = 'customData.simCmd.stringEscapeSpecials',
    },
    {
        label = 'Map/array rendering: sort keys by name',
        enabled = true,
        checkable = true,
        checked = true,
        propertyName = 'customData.simCmd.mapSortKeysByName',
    },
    {
        label = 'Map/array rendering: sort keys by type',
        enabled = true,
        checkable = true,
        checked = true,
        propertyName = 'customData.simCmd.mapSortKeysByType',
    },
    {
        label = 'Map/array rendering: shadow long strings',
        enabled = true,
        checkable = true,
        checked = true,
        propertyName = 'customData.simCmd.mapShadowLongStrings',
    },
    {
        label = 'Map/array rendering: shadow buffer strings',
        enabled = true,
        checkable = true,
        checked = true,
        propertyName = 'customData.simCmd.mapShadowBufferStrings',
    },
    {
        label = 'Map/array rendering: shadow strings with special characters',
        enabled = true,
        checkable = true,
        checked = true,
        propertyName = 'customData.simCmd.mapShadowSpecialStrings',
    },
    ]]--
    {
        label = 'History: skip repeated commands',
        enabled = true,
        checkable = true,
        checked = true,
        propertyName = 'customData.simCmd.historySkipRepeated',
    },
    {
        label = 'History: remove duplicates',
        enabled = true,
        checkable = true,
        checked = true,
        propertyName = 'customData.simCmd.historyRemoveDups',
    },
    {
        label = 'History: show matching entries (select with UP)',
        enabled = true,
        checkable = true,
        checked = false,
        propertyName = 'customData.simCmd.showMatchingHistory',
    },
    {
        label = 'Set convenience vars',
        enabled = true,
        checkable = true,
        checked = true,
        propertyName = 'customData.simCmd.setConvenienceVars',
    },
    {
        label = 'Auto-accept common completion prefix',
        enabled = true,
        checkable = true,
        checked = true,
        propertyName = 'customData.simCmd.autoAcceptCommonCompletionPrefix',
    },
    {
        label = 'Show full stack traceback',
        enabled = true,
        checkable = true,
        checked = true,
        propertyName = 'customData.simCmd.showFullStackTrace',
    },
}
-- LuaFormatter on

menuByPropertyName = {}
menuByHandle = {}

function menuItemState(menu)
    local state = 0
    if menu.enabled then state = state + 1 end
    if menu.checked then state = state + 2 end
    if menu.checkable then state = state + 4 end
    return state
end

function sysCall_info()
    return {
        menu = 'Developer tools\nCommander\nCommander',
    }
end

function sysCall_init()
    simCmd = require 'simCmd'
    simCmd.setVisible(true)

    local filterPropertyNames = {}

    for i, menu in ipairs(menus) do
        menu.handle = sim.moduleEntry(-1, 'Developer tools\nCommander\n' .. menu.label, menuItemState(menu))
        menuByHandle[menu.handle] = menu
        if menu.propertyName then
            menuByPropertyName[menu.propertyName] = menu
            table.insert(filterPropertyNames, menu.propertyName)
        end
    end

    sim.setEventFilters{[sim.handle_app] = filterPropertyNames}
end

function sysCall_moduleEntry(inData)
    local menu = menuByHandle[inData.handle]
    if not menu then return end
    if menu.checkable then
        menu.checked = not menu.checked
        if menu.propertyName then
            sim.setBoolProperty(sim.handle_app, menu.propertyName, menu.checked)
        end
    end
    if menu.menuCallback then
        menu.menuCallback(menu)
    end
    sim.moduleEntry(menu.handle, nil, menuItemState(menu))
end

function sysCall_event(eventData)
    local cbor = require 'simCBOR'
    events = cbor.decode(eventData)
    for _, e in ipairs(events) do
        if e.handle == sim.handle_app and e.event == 'objectChanged' then
            for k, v in pairs(e.data) do
                local menu = menuByPropertyName[k]
                if menu and menu.checkable and type(v) == 'boolean' then
                    menu.checked = v
                    sim.moduleEntry(menu.handle, nil, menuItemState(menu))
                end
            end
        end
    end
end

function sysCall_addOnScriptSuspend()
    return {cmd = 'cleanup'}
end

function sysCall_cleanup()
    simCmd.setVisible(false)
    unloadPlugin(simCmd)
end

require('addOns.autoStart').setup{ns = 'simCmd', default = true}
