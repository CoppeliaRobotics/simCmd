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
    for i,t in ipairs(ast) do
        if ast.tag=='Call' and t.pos<=pos and pos<=t.end_pos then
            local fn=getCompoundId(ast[1])
            local argindex=i-1
            table.insert(results,{fn,argindex})
        end
        findCallsAtPosition(t,pos,results)
    end
    return results
end

function getCallContexts(s,pos)
    s1,ast=parseIncomplete(s)
    --print(ast)
    print('')
    print(s)
    rs=findCallsAtPosition(ast,#s)
    for i,r in ipairs(rs) do
        print(r[1],r[2])
    end
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
end

sysCall_init()
