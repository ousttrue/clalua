clalua = require "clalua"

lrdb = require("lrdb_server")
lrdb.activate(21110) --21110 is using port number. waiting for connection by debug client.

function printf(fmt, ...)
    print(string.format(fmt, ...))
end

--
-- -X => opt[X] = true
-- -X 1 => opt[X] = 1
-- -XY => error. use --XY
-- --YY => opt[YY] = true
-- --YY "hello" => opt[YY] = "hello"
--
function getopt(arg)
    local opt = {}
    function push_value(key, value)
        local lastvalue = opt[key]
        if lastvalue then
            if type(lastvalue) == "table" then
                if lastvalue[#lastvalue] == true then
                    -- replace
                    lastvalue[#lastvalue] = value
                else
                    -- push
                    table.insert(lastvalue, value)
                end
            else
                if lastvalue == true then
                    -- replace
                    opt[key] = value
                else
                    -- push
                    opt[key] = {
                        lastvalue,
                        value
                    }
                end
            end
        else
            opt[key] = value
        end
    end

    local lastkey = nil
    for k, v in ipairs(arg) do
        if string.sub(v, 1, 2) == "--" then
            lastkey = string.sub(v, 3)
            push_value(lastkey, true)
        elseif string.sub(v, 1, 1) == "-" then
            key = string.sub(v, 2)
            if string.len(key) > 1 then
                error(string.format('"%s" option name must 1 length. use "-%s"', v, v))
            end
            lastkey = key
            push_value(lastkey, true)
        else
            if lastkey then
                push_value(lastkey, v)
            end
            lastkey = nil
        end
    end
    return opt
end

function print_table(t, indent)
    indent = indent or ""
    for k, v in pairs(t) do
        printf("%s%s => %s", indent, k, v)
        if type(v) == "table" then
            print_table(v, indent .. "  ")
        end
    end
end

function writeln(f, text)
    if not f then
        error("no f: " .. text)
    end
    if text then
        f:write(text)
    end
    f:write("\n")
end

function writef(f, fmt, ...)
    f:write(string.format(fmt, ...))
end

function writefln(f, fmt, ...)
    f:write(string.format(fmt, ...))
    f:write("\n")
end

function rfind(src, pred)
    local i = nil
    -- print(src)
    for i = #src, 1, -1 do
        local c = string.sub(src, i, i)
        -- printf("%d %s", i, c)
        if pred(c) then
            -- printf("found: %s %d", src, i)
            return i
        end
    end
end

function basename(src)
    local function pred(c)
        if c == "/" or c == "\\" then
            return true
        end
    end
    local i = rfind(src, pred)
    if i then
        return string.sub(src, i + 1)
    end
    return src
end

function dirname(src)
    local function pred(c)
        if c == "/" or c == "\\" then
            return true
        end
    end
    local i = rfind(src, pred)
    if i then
        return string.sub(src, 1, i - 1)
    end
    return src
end

function isFirstAlpha(src)
    return string.match(src, "^%a")
end

function ClangParse(option)
    local headers = option.headers or {}
    local includes = option.includes or {}
    local defines = option.defines or {}
    local externC = option.externC or false
    local isD = option.isD or false
    local sourceMap = clalua.parse(headers, includes, defines, externC, isD)
    if sourceMap.empty then
        return nil
    end
    return sourceMap
end

function startswith(src, start)
    if type(start) == "string" then
        start = {
            start
        }
    end

    for _, s in ipairs(start) do
        local found = string.find(src, s)
        if found and found == 1 then
            -- printf("%s start with %s: %d", src, start, found and found or -1)
            return true
        end
    end

    return false
end

lfs = require "lfs"

function getPath(str)
    local m = str:match("(.*)[/\\]")
    return m
end
function mkdirRecurse(path)
    if lfs.attributes(path) then
        return
    end

    local dir = getPath(path)
    if lfs.attributes(dir) == nil then
        mkdirRecurse(dir)
    end

    local result = lfs.mkdir(path)
end

_G['file'] = {
    exists = function(path)
        return lfs.attributes(path) ~= nil
    end,
    isDir = function(path)
        local attr = lfs.attributes(path)
        if not attr then
            return false
        end
        return attr.mode == "directory"
    end,
    mkdirRecurse = mkdirRecurse,
    rmdirRecurse = function(path)
        return lfs.rmdir(path)
    end,
    ls = function(path)
        local files = {}
        for file in lfs.dir(path) do
            if file ~= "." and file ~= ".." then
                table.insert(files, path .. '/' .. file)
            end
        end
        return files
    end
}
