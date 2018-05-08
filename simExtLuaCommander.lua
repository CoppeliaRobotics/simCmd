local simLuaCmd={}

html_help = [[
        <h2>LuaCommander plugin</h2>
        Use this plugin for quick evaluation of Lua expressions.

        <h3>Keyboard Shortcuts</h3>
        Press Ctrl+Alt+C to focus the text input.

        <h3>Completion</h3>
        Begin to type the name of a function (e.g. "sim.getObjectHa") and press TAB to automatically complete it. If there are multiple matches, repeatedly press TAB to cycle through completions, and Shift+TAB to cycle back.

        <h3>String Rendering Flags</h3>
        There are some flags that control how the results are displayed. Those are input by adding a comment at the end of the line, containing as comma separated list of key=value pairs, e.g.: "luaExpression --flag1=10,flag2=test". Flags can be abbreviated by typing only the initial part, e.g. "pre" instead of "precision", down to any length, provided it is not ambiguous.
        <ul>
            <li><b>depth</b>: (integer) limit the maximum depth when rendering a map-table.</li>
            <li><b>precision</b>: (integer) number of floating point digits.</li>
            <li><b>sort</b>: (k, t, tk, off) how to sort map-table entries:
                    k: sort by keys;
                    t: sort by type;
                    tk: sort by type, then by key;
                    off: don't sort at all.
            </li>
        </ul>
]]

function help()
    ui = simUI.create([[<ui title="LuaCommander Plugin" closeable="true" size="400,500">
        <text-browser text="]]..string.gsub(html_help,'"','&quot;')..[[" />
    </ui>]])
end

return simLuaCmd
