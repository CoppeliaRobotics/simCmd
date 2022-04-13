local parser=require'lua-parser.parser'
local pp=require'lua-parser.pp'

local function parseIncomplete(s,f,d)
    d=1+(d or 0)

    local ast,error_msg=parser.parse(s,f)
    if ast then
        return s,ast
    end

    sym=error_msg:match" expected '(.*)' "
    if sym then
        return parseIncomplete(s..' '..sym,f,d)
    end

    if error_msg:match' unclosed string%f[%A]' then
        -- ' or " ???
        local choices=d%2>0 and {"'",'"'} or {'"',"'"}
        local s1,a=parseIncomplete(s..choices[1],f,d)
        if a then
            return s1,a
        else
            local s2,b=parseIncomplete(s..choices[2],f,d)
            if b then
                return s2,b
            end
        end
    end

    if error_msg:match' expected a numeric or generic range ' then
        return parseIncomplete(s..' i=0,1,1 ',f,d)
    end

    if error_msg:match' expected a condition after ' then
        return parseIncomplete(s..' false ',f,d)
    end

    if error_msg:match' expected an expression ' then
        return parseIncomplete(s..' 0 ',f,d)
    end

    error(error_msg)
end

local function getCompoundId(ast)
    if type(ast)~='table' then return ast end
    if ast.tag=='Id' then return ast[1] end
    if ast.tag=='String' then return ast[1] end
    if ast.tag=='Index' and ast[3]==nil then
        return getCompoundId(ast[1])..'.'..getCompoundId(ast[2])
    end
end

local function findCallsAtPosition(ast,pos,results)
    results=results or {}
    if type(ast)~='table' then return end
    local fn=''
    if ast.tag=='Call' then
        fn=getCompoundId(ast[1])
    end
    for i,t in ipairs(ast) do
        if type(t)=='table' then
            if type(t)=='table' and ast.tag=='Call' and t.pos and t.end_pos and t.pos<=pos and pos<=t.end_pos then
                local argindex=i-1
                table.insert(results,{fn,argindex})
            end
            findCallsAtPosition(t,pos,results)
        end
    end
    -- if no call context are being returned for <fn>...
    -- but anyway we are in a call, so return <fn,1>:
    if ast.tag=='Call' and ast.pos<=pos and pos<=ast.end_pos then
        local havefn=false
        for i=1,#results do
            if results[i][1]==fn then
                havefn=true
                break
            end
        end
        if not havefn then
            print('adding extra',fn)
            table.insert(results,{fn,1})
        end
    end
    return results
end

function dump(o,indent)
    indent=indent or ''
    if type(o)=='table' then
        local s='{\n'
        for k,v in pairs(o) do
            if type(k)=='number' then k='['..k..']' end
            s = s..indent..'    '..k..'='..dump(v,indent..'    ')..',\n'
        end
        return s..indent..'}'
    else
        return tostring(o)
    end
end

function getCallContexts(s,pos)
    s1,ast=parseIncomplete(s)
    rs=findCallsAtPosition(ast,#s)
    return rs
end

local function test(s)
    getCallContexts(s,#s)
end

local function sysCall_init()
    test('if f("x')
    test('if f(\'x')
    test('for ')
    test('while ')
    test('if ')
    test('if (function() return 1')
    test('if (function() return 1 end)(')
    test('sim.setObjectAlias(sim.getObject("a"),"b')
    test('sim.setObjectPosition(h,-')
    test('sim.setObjectPosition(h,-1,{')
    test('sim.getObject(names[')
    test('sim.foo(x..')
    test('sim.foo(a,b,c,sim.bar(x+')
    test('sim.getObject(')
    test('sim.getObjectAlias(0,sim.getObject(),{')
end

sysCall_init()

return getCallContexts
