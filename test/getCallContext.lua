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

local function transformTree(ast,d)
    if type(ast)~='table' then
        return ast
    end

    if ast.tag=='Index' and ast[1].tag=='Id' and ast[2].tag=='String' and ast[3]==nil then
        return {tag='IdDot',pos=ast.pos,end_pos=ast.end_pos,[1]=ast[1][1]..'.'..ast[2][1]}
    end

    local ast1={tag=ast.tag,pos=ast.pos,end_pos=ast.end_pos}
    for i,t in ipairs(ast) do
        local t1=transformTree(t,d)

        if ast1.tag=='Call' and t1.tag=='IdDot' then
            ast1.f=t1[1]
        else
            table.insert(ast1,t1)
        end
    end

    return ast1
end

local function findCallsAtPosition(ast,pos,results)
    results=results or {}
    if type(ast)~='table' then return end
    for i,t in ipairs(ast) do
        if ast.tag=='Call' and ast.f and t.pos<=pos and pos<=t.end_pos then
            table.insert(results,{ast.f,i})
        end
        findCallsAtPosition(t,pos,results)
    end
    return results
end

local function visitTree(ast,d)
    d=(d or 0)+1
    s='';for i=1,d do s=s..'    ' end

    if type(ast)~='table' then
        print(s..'visitTree:',ast)
        return
    end

    print(s..'visitTree:',ast.tag,ast.pos,ast.end_pos,ast.f)
    for i,t in ipairs(ast) do
        visitTree(t,d)
    end
end

function getCallContexts(s,pos)
    s1,ast=parseIncomplete(s)
    ast=transformTree(ast)
    --print(ast)
    --visitTree(ast)
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
