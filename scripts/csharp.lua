local HEADLINE =
    [[
// This source code was generated by clalua"
using System;
using System.Runtime.InteropServices;
]]

local INT_MAX = 2147483647

--
-- ref.type.class から得る
--
local PRIMITIVE_MAP = {
    Void = 'void',
    Bool = 'bool',
    --
    Int8 = 'sbyte',
    Int16 = 'short',
    Int32 = 'int',
    Int64 = 'long',
    --
    UInt8 = 'byte',
    UInt16 = 'ushort',
    UInt32 = 'uint',
    UInt64 = 'ulong',
    --
    Float = 'float',
    Double = 'double'
}

--
-- ref.type.name から得る
--
local NAME_MAP = {
    ID3DInclude = 'IntPtr',
    _GUID = 'Guid',
    _D3DCOLORVALUE = 'System.Numerics.Vector4',
    --
    D2D_POINT_2F = 'System.Numerics.Vector2',
    D2D1_POINT_2F = 'System.Numerics.Vector2',
    D2D1_COLOR_F = 'System.Numerics.Vector4',
    D2D1_RECT_F = 'System.Numerics.Vector4',
    D2D_MATRIX_3X2_F = 'System.Numerics.Matrix3x2',
    D2D1_MATRIX_3X2_F = 'System.Numerics.Matrix3x2'
}

local FIELD_MAP = {
    LPCSTR = {
        'string',
        {
            attr = '[MarshalAs(UnmanagedType.LPStr)]'
        }
    },
    LPCWSTR = {
        'string',
        {
            attr = '[MarshalAs(UnmanagedType.LPWStr)]'
        }
    }
}

local PARAM_MAP = {
    LPCSTR = {
        'string',
        {
            attr = '[MarshalAs(UnmanagedType.LPStr)]'
        }
    },
    LPCWSTR = {
        'ref ushort',
        {
            isRef = true
        }
    }
}

local ESCAPE_SYMBOLS = {
    --
    ref = true,
    ['in'] = true,
    event = true,
    string = true
}

local function CSEscapeName(src, i)
    i = i or ''
    if ESCAPE_SYMBOLS[src] then
        return '_' .. src
    end
    if #src == 0 then
        src = string.format('__param__%s', i)
    end
    return src
end

local function isInterface(decl)
    -- decl = decl.typedefSource

    if decl.class ~= 'Struct' then
        return false
    end

    if decl.definition then
        -- resolve forward decl
        decl = decl.definition
    end

    return decl.isInterface
end

local pointerTypes = {
    'HDC__',
    'HWND__',
    'HINSTANCE__',
    'HKL__',
    'HICON__',
    'RAWINPUT',
    'HMENU__',
    'HWINEVENTHOOK__',
    'HMONITOR__'
}

local function isPointer(name)
    for i, key in ipairs(pointerTypes) do
        if name == key then
            return true
        end
    end
end

local function isUserType(t)
    for i, u in ipairs {'Enum', 'Struct', 'TypeDef', 'Function'} do
        if u == t.class then
            return true
        end
    end
end

local function resolveTypedef(t)
    if t.class == 'TypeDef' then
        local resolved = resolveTypedef(t.ref.type)
        if isUserType(resolved) then
            if t.useCount == 1 and resolved.name ~= t.name then
            -- rename
            -- printf("tag rename %s => %s", resolved.name, t.name)
            -- TODO: __newindex
            -- resolved.name = t.name
            end
        end
        return resolved
    else
        return t
    end
end

local function getMapType(t, isParam)
    local name = PRIMITIVE_MAP[t.class]
    if name then
        return name, {}
    end

    if isUserType(t) then
        local name = NAME_MAP[t.name]
        if name then
            return name, {}
        end

        if isParam then
            local nameOption = PARAM_MAP[t.name]
            if nameOption then
                return table.unpack(nameOption)
            end
        else
            local nameOption = FIELD_MAP[t.name]
            if nameOption then
                return table.unpack(nameOption)
            end
        end
    end
end

local function isCallback(t)
    if t.class == 'Function' then
        return true
    end

    if t.class == 'TypeDef' then
        if t.ref.type.class == 'Function' then
            -- return t.name
            error('not implemented')
        end
        if t.ref.type.class == 'Pointer' and t.ref.type.ref.type.class == 'Function' then
            return true
        end
    end
end

local function CSType(t, isParam)
    local typeName, option = getMapType(t, isParam)
    if typeName then
        return typeName, option
    end

    if isCallback(t) then
        return t.name, {}
    end

    -- typedefを剥がす
    t = resolveTypedef(t)

    local typeName, option = getMapType(t, isParam)
    if typeName then
        if not option then
            error('no option')
        end
        return typeName, option
    end

    if isUserType(t) then
        local name = NAME_MAP[t.name]
        if name then
            return name, {}
        end

        if isParam then
            local nameOption = PARAM_MAP[t.name]
            if nameOption then
                return table.unpack(nameOption)
            end
        else
            local nameOption = FIELD_MAP[t.name]
            if nameOption then
                return table.unpack(nameOption)
            end
        end
    end

    if t.class == 'Pointer' then
        -- csharpで型を表現できる場合は、ref, out に。
        -- できない場合は、IntPtrにする
        local option = {isConst = t.ref.isConst, isRef = true}
        local typeName, refOption = CSType(t.ref.type, isParam)
        for k, v in pairs(refOption) do
            option[k] = option[k] or v
        end

        -- 特定の型へのポインターは、IntPtrにする
        if typeName == 'void' then
            return 'IntPtr', option
        end
        if isPointer(t.ref.type.name) then
            -- HWND etc...
            return 'IntPtr', option
        end

        if isInterface(t.ref.type) then
            option.isCom = true
            -- ref, out を付けない
            return typeName, option
        end

        if isParam then
            if option.isCom then
                option.attr =
                    string.format(
                    '[MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(CustomMarshaler<%s>))]',
                    typeName
                )
            end
            local inout = option.isConst and 'ref' or 'out'
            if inout == 'out' and typeName == 'IUnknown' then
                option.isCom = false
                option.attr = nil
                typeName = 'IntPtr'
            end
            if startswith(typeName, {'out ', 'ref '}) then
                -- double pointer
                typeName = 'IntPtr'
            end
            typeName = string.format('%s %s', inout, typeName)
            return typeName, option
        else
            return 'IntPtr', option
        end
    elseif t.class == 'Reference' then
        local option = {isConst = t.ref.isConst, isRef = true}
        local typeName, refOption = CSType(t.ref.type, isParam)
        for k, v in pairs(refOption) do
            option[k] = option[k] or v
        end
        local inout = option.isConst and 'ref' or 'out'
        return string.format('%s %s', inout, typeName), option
    elseif t.class == 'Array' then
        -- return DArray(t)
        local a = t
        local typeName, option = CSType(a.ref.type, isParam)
        option.isConst = option.isConst or t.ref.isConst
        if isParam then
            option.isRef = true
            return string.format('ref %s', typeName), option
        else
            option.attr = string.format('[MarshalAs(UnmanagedType.ByValArray, SizeConst=%d)]', a.size)
            return string.format('%s[]', typeName), option
        end
    else
        if #t.name == 0 then
            return nil, {}
        end
        return t.name, {}
    end
end

local function CSTypedefDecl(f, t)
    if t.ref.type.class == 'Function' then
        -- ありえない？
        eroor('not implemented')
    end

    local dst, option = CSType(t.ref.type)
    if not dst then
        -- ありえない？
        -- error('not implemented')
        -- return
        dst = t.name
    end

    if t.ref.type.class == 'Pointer' and t.ref.type.ref.type.class == 'Function' then
        -- 関数ポインターへのTypedef --
        local decl = t.ref.type.ref.type
        local retType = CSType(decl.ret.type)
        local params = decl.params
        local paramTypes = {}
        for i, param in ipairs(params) do
            local comma = i == #params and '' or ','
            local dst, option = CSType(param.ref.type, true)
            local paramName = CSEscapeName(param.name, i)
            local typeName = string.format('%s %s', dst, paramName)
            table.insert(paramTypes, typeName)
        end
        writefln(f, '    public delegate %s %s(%s);', retType, t.name, table.concat(paramTypes, ', '))
    end

    -- do nothing
    -- writefln(f, "    public struct %s { public %s Value; } // %d", t.name, dst, t.useCount)
end

local function CSEnumDecl(sourceDir, decl, option, indent)
    if #decl.name == 0 then
        -- writefln(f, "    // enum nameless", indent)
        return
    end

    if option.omitEnumPrefix then
        -- TODO:
    end

    local path = string.format('%s/%s.cs', sourceDir, decl.name)
    local f = io.open(path, 'w')
    writeln(f, HEADLINE)
    writefln(f, 'namespace %s {', option.packageName)

    writefln(f, '%spublic enum %s // %d', indent, decl.name, decl.useCount)
    writefln(f, '%s{', indent)
    for i, value in ipairs(decl.values) do
        if value.value > INT_MAX then
            writefln(f, '%s    %s = unchecked((int)0x%x),', indent, value.name, value.value)
        else
            writefln(f, '%s    %s = 0x%x,', indent, value.name, value.value)
        end
    end
    writefln(f, '%s}', indent)
    writeln(f, '}')
    io.close(f)
end

local function CSGlobalFunction(f, decl, indent, option, sourceName)
    local dllName = option.dll_map[sourceName] or sourceName
    if decl.isExternC then
        writefln(f, '%s[DllImport("%s.dll")]', indent, dllName)
    else
        -- mangle
        writefln(f, '%s[DllImport("%s.dll")]', indent, dllName)
    end
    writefln(f, '%spublic static extern %s %s(', indent, CSType(decl.ret.type), decl.name)
    local params = decl.params
    for i, param in ipairs(params) do
        local comma = i == #params and '' or ','
        local dst, option = CSType(param.ref.type, true)
        writefln(f, '%s    %s%s %s%s', indent, option.attr or '', dst, CSEscapeName(param.name, i), comma)
        -- TODO: dfeault value = getValue(param, option.param_map)
    end
    writefln(f, '%s);', indent)
    if option.overload then
        local overload = option.overload[decl.name]
        if overload then
            writefln(f, overload)
        end
    end
end

local function CSInterfaceMethod(f, decl, indent, option, vTableIndex, override)
    local ret = CSType(decl.ret.type)
    local name = decl.name
    if name == 'GetType' then
        name = 'GetComType'
    end
    if name == 'DrawTextA' or name == 'DrawTextW' then
        -- avoid DrawText macro
        -- d2d1 work around
        name = 'DrawText'
    end

    writefln(f, '%spublic %s %s %s(', indent, override, ret, name)
    local params = decl.params
    local callbackParams = {'m_ptr'}
    local delegateParams = {}
    local callvariables = {}
    for i, param in ipairs(params) do
        local comma = i == #params and '' or ','
        local dst, option = CSType(param.ref.type, true)
        local name = CSEscapeName(param.name)
        if option.isCom then
            if param.ref.type.class == 'Pointer' and param.ref.type.ref.type.class == 'Pointer' then
                if not option.isConst then
                    -- out interface
                    writefln(f, '%s    %s %s%s', indent, dst, name, comma)
                    table.insert(
                        callvariables,
                        string.format('%s = new %s();', name, dst:gsub('^ref ', ''):gsub('^out ', ''))
                    )
                    table.insert(callbackParams, string.format('out %s.PtrForNew', name))
                    table.insert(delegateParams, string.format('out IntPtr %s', name))
                else
                    -- may interface array
                    -- printf("## %s %d ##", decl.name, i)
                    -- print_table(option)
                    writefln(f, '%s    ref IntPtr %s%s', indent, name, comma)
                    table.insert(callbackParams, string.format('ref %s', name))
                    table.insert(delegateParams, string.format('ref IntPtr %s', name))
                end
            else
                -- in interface
                writefln(f, '%s    %s %s%s', indent, dst, name, comma)
                table.insert(callbackParams, string.format('%s!=null ? %s.Ptr : IntPtr.Zero', name, name))
                table.insert(delegateParams, string.format('IntPtr %s', name))
            end
        elseif option.isRef then
            writefln(f, '%s    %s %s%s', indent, dst, name, comma)
            local isRef = string.sub(dst, 1, 4)
            if isRef == 'ref ' then
                table.insert(callbackParams, string.format('ref %s', name))
            elseif isRef == 'out ' then
                table.insert(callbackParams, string.format('out %s', name))
            else
                table.insert(callbackParams, string.format('%s', name))
            end
            table.insert(delegateParams, string.format('%s %s', dst, name))
        else
            writefln(f, '%s    %s %s%s', indent, dst, name, comma)
            table.insert(callbackParams, string.format('%s', name))
            table.insert(delegateParams, string.format('%s %s', dst, name))
        end
        -- TODO: default value = getValue(param, option.param_map)
    end
    local delegateName = decl.name .. 'Func'
    writefln(
        f,
        [[
        ){
            var fp = GetFunctionPointer(%s);
            if(m_%s==null) m_%s = (%s)Marshal.GetDelegateForFunctionPointer(fp, typeof(%s));
            %s
            %sm_%s(%s);
        }]],
        vTableIndex,
        delegateName, -- m_%s
        delegateName, -- m_%s
        delegateName, -- (%s)
        delegateName, -- typeof(%s)
        table.concat(callvariables, ''),
        ret == 'void' and '' or 'return ', -- %s
        delegateName, -- m_%s
        table.concat(callbackParams, ', ') -- (%s)
    )

    -- delegate
    writef(f, '%sdelegate %s %s(IntPtr self', indent, ret, delegateName)
    for i, param in ipairs(delegateParams) do
        writef(f, ', %s', param)
        -- TODO: default value = getValue(param, option.param_map)
    end
    writeln(f, ');')
    writefln(f, '%s%s m_%s;', indent, delegateName, delegateName)
    writeln(f)
end

local function getStruct(decl)
    if not decl then
        return nil
    end
    if decl.class == 'Struct' then
        if decl.isForwardDecl then
            decl = decl.definition
        end
        return decl
    elseif decl.class == 'TypeDef' then
        return getStruct(decl.ref.type)
    else
        error('XXXXX')
    end
end

local function hasSameMethod(name, decl)
    local current = decl
    while true do
        current = getStruct(current.base)
        if not current then
            break
        end

        for i, method in ipairs(current.methods) do
            if method.name == name then
                -- print("found", name)
                return true
            end
        end
    end
end

local anonymousMap = {}

local function CSComInterface(sourceDir, decl, option)
    if not decl.isInterface then
        error('is not interface')
    end

    -- com interface
    if decl.isForwardDecl then
        return
    end

    local name = decl.name

    local path = string.format('%s/%s.cs', sourceDir, decl.name)
    local f = io.open(path, 'w')
    writeln(f, HEADLINE)
    writefln(f, 'namespace %s {', option.packageName)

    -- interface
    writef(f, '    public class %s', name)
    if not decl.base then
        writef(f, ': ComPtr')
    else
        writef(f, ': %s', decl.base.name)
    end

    writeln(f)
    writeln(f, '    {')
    if decl.iid then
        -- writefln(f, '    [Guid("%s"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]', decl.iid)
        writefln(
            f,
            [[
        static Guid s_uuid = new Guid("%s");
        public static new ref Guid IID =>ref s_uuid;
        public override ref Guid GetIID(){ return ref s_uuid; }
                ]],
            decl.iid
        )
    end

    -- methods
    local indices = decl.vTableIndices
    local baseMaxIndices = -1
    if decl.base then
        baseMaxIndices = 0
        for i, index in ipairs(decl.base.vTableIndices) do
            if index > baseMaxIndices then
                baseMaxIndices = index
            end
        end
    end
    if baseMaxIndices >= 0 then
    -- printf("# %s: baseMaxIndices = %d", decl.name, baseMaxIndices)
    end
    local used = {}
    for i, method in ipairs(decl.methods) do
        local index = indices[i]
        if index <= baseMaxIndices then
            -- override. not necessary
            -- printf("[%d] override:%d<=%d %s", i, index, baseMaxIndices, method.name)
        else
            if used[method.name] then
                printf('[%d] duplicate:%d %s', i, index, method.name)
            else
                CSInterfaceMethod(f, method, '        ', option, index, 'virtual')
                used[method.name] = true
            end
        end
    end
    writeln(f, '    }')
    writeln(f, '}')
    io.close(f)
end

local function CSStructDecl(f, decl, option, i)
    -- assert(!decl.m_forwardDecl);
    local name = decl.name
    if not name or #name == 0 then
        -- writeln(f, "    // struct nameless")
        -- return
        name = string.format('%s_anonymous_%d', table.concat(decl.namespace, '_'), i)
        -- print(name)
        anonymousMap[decl.hash] = name
    end

    if decl.isForwardDecl then
        -- forward decl
        if #decl.fields > 0 then
            error('forward decl has fields')
        end
        writefln(f, '    // forward declaration %s;', name)
    else
        if decl.isUnion then
            writeln(f, '    [StructLayout(LayoutKind.Explicit)]')
        else
            writeln(f, '    [StructLayout(LayoutKind.Sequential)]')
        end
        writefln(f, '    public struct %s // %d', name, decl.useCount)
        writeln(f, '    {')
        for i, field in ipairs(decl.fields) do
            local typeName, option = CSType(field.ref.type, false)
            if not typeName then
                typeName = anonymousMap[field.ref.type.hash]
            end
            if not typeName then
                local fieldType = field.ref.type
                if fieldType.class == 'Struct' then
                    if fieldType.isUnion then
                        -- for i, unionField in ipairs(fieldType.fields) do
                        --     local unionFieldTypeName = CSType(unionField.ref.type)
                        --     writefln(f, "        %s %s;", unionFieldTypeName, CSEscapeName(unionField.name))
                        -- end
                        -- writefln(f, "    }")
                        writefln(f, '        // anonymous union')
                    else
                        writefln(f, '       // anonymous struct %s;', CSEscapeName(field.name))
                    end
                else
                    error(string.format('unknown: %s', fieldType))
                end
            else
                if decl.isUnion then
                    writefln(f, '        [FieldOffset(0)]', field.offset)
                end
                local name = CSEscapeName(field.name, i)
                if #name == 0 then
                    name = string.format('__anonymous__%d', i)
                end
                writefln(f, '        %spublic %s %s;', option.attr or '', typeName, name)
            end
        end

        writeln(f, '    }')
    end
end

local function CSDecl(f, decl, option, i, sourceDir)
    local hasComInterface = false
    if decl.class == 'TypeDef' then
        CSTypedefDecl(f, decl)
    elseif decl.class == 'Enum' then
        CSEnumDecl(sourceDir, decl, option, '    ')
    elseif decl.class == 'Function' then
        error('not reach Function')
    elseif decl.class == 'Struct' then
        hasComInterface = decl.isInterface
        if hasComInterface then
            CSComInterface(sourceDir, decl, option)
        else
            CSStructDecl(f, decl, option, i)
        end
    else
        error('unknown', decl)
    end
    return hasComInterface
end

local function getPointerValue(str)
    for i, key in ipairs {'HWND', 'HBITMAP', 'HANDLE'} do
        local pattern = '(.*)%( *' .. key .. ' *%)(.*)'
        local s, e = string.match(str, pattern)
        if s then
            return s .. e
        end
    end
end

local function trim(s)
    return s:match '^%s*(.-)%s*$'
end

local function CSMacro(f, macro, macro_map)
    if macro.isFunctionLike then
        writefln(f, '        // macro function: %s', table.concat(macro.tokens, ' '))
        return
    end

    local text = macro_map[macro.name]
    if text then
        writefln(f, '        %s', text)
        return
    end

    local tokens = macro.tokens
    table.remove(tokens, 1)

    if isFirstAlpha(tokens[1]) then
        writefln(f, '        // unknown type: %s', table.concat(macro.tokens, ' '))
        return
    end

    -- const definitions
    local value = table.concat(tokens, ' ')

    for i, key in ipairs {'LONG', 'DWORD', 'int'} do
        value = value:gsub('%( ' .. key .. ' %)', '')
    end

    local pointerValue = getPointerValue(value)
    if pointerValue then
        -- HWND, HBITMAP...
        writefln(f, '        public static readonly IntPtr %s = new IntPtr(%s);', macro.name, pointerValue)
        return
    end

    for i, pattern in ipairs {'^(0x)([0-9a-fA-F]+)(U*)(L*)$', '%( *(0x)(%w+) %)'} do
        local hex, n, u, l = string.match(value, pattern)
        if n and #n > 0 then
            -- hex
            if l and #l > 1 then
                error('not implemented: ' .. l)
            end

            if u and #u > 0 then
                -- unsigned
                writefln(f, '        public const uint %s = %s%s;', macro.name, hex, n)
                return
            else
                -- signed
                writefln(f, '        public const int %s = unchecked((int)%s%s);', macro.name, hex, n)
                return
            end
        end
    end

    local valueType = 'int'
    if string.find(value, '%"') then
        valueType = 'string'
    elseif string.find(value, 'f') and not string.find(value, '0x') then
        valueType = 'float'
    elseif string.find(value, '%.') then
        valueType = 'double'
    elseif string.find(value, 'WS_') then
        valueType = 'long'
    elseif string.find(value, 'DS_') then
        valueType = 'long'
    end
    writefln(f, '        public const %s %s = %s;', valueType, macro.name, value)
end

local function CSSource(f, source, option)
    -- dir
    local sourceDir = string.format('%s/%s', option.dir, source.name)
    file.mkdirRecurse(sourceDir)

    -- open
    local path = sourceDir .. '.cs'
    printf('writeTo: %s', path)
    local f = io.open(path, 'w')

    macro_map = option['macro_map'] or {}
    declFilter = option['filter']
    omitEnumPrefix = option['omitEnumPrefix']

    writeln(f, HEADLINE)
    writefln(f, 'namespace %s {', option.packageName)

    if option.injection then
        local inejection = option.injection[source.name]
        if inejection then
            writefln(f, inejection)
        end
    end

    -- const
    if #source.macros > 0 then
        -- enum じゃなくて const 変数に(castを避けたい)
        local function CSMacroEnum(path, prefix, items, const_option)
            if #items == 0 then
                return
            end

            local type = const_option.type or 'int'
            local pred = const_option.value

            local f = io.open(path, 'w')
            writeln(f, HEADLINE)
            writefln(f, 'namespace %s {', option.packageName)

            writefln(f, '    public static partial class %s {', prefix)
            for i, m in ipairs(items) do
                local text = option.macro_map[m.name]
                if text then
                    writefln(f, '        %s', text)
                else
                    local tokens = m.tokens
                    table.remove(tokens, 1)
                    local name = string.sub(m.name, #prefix + 1)
                    writefln(
                        f,
                        '        public const %s %s = %s;',
                        type,
                        name,
                        pred and pred(prefix, tokens, const_option.value_map) or table.concat(tokens, ' ')
                    )
                end
            end
            writeln(f, '    }')

            writeln(f, '}')
            io.close(f)
        end

        writeln(f, '    public static partial class Constants {')
        local macros = source.macros

        -- option.constに設定があるものだけ、定数宣言を別ファイルに分離する
        local used = {}
        for prefix, const in pairs(option.const) do
            local group = {}
            local match = prefix .. '_'
            if const.match then
                match = const.match
            end
            for i, macro in ipairs(macros) do
                if startswith(macro.name, match) then
                    table.insert(group, macro)
                    used[i] = true
                end
            end
            local path = string.format('%s/%s.cs', sourceDir, prefix)
            CSMacroEnum(path, prefix, group, const)
        end

        local constants = {}
        for i, macro in ipairs(macros) do
            if not used[i] then
                if constants[macro.name] then
                    writefln(f, '// duplicate: %s = %s', macro.name, table.concat(macro.tokens, ' '))
                else
                    CSMacro(f, macro, macro_map)
                    constants[macro.name] = true
                end
            end
        end
        writeln(f, '    }')
    end

    -- types
    local hasComInterface = false
    local funcs = {}
    local types = {}
    for i, decl in ipairs(source.types) do
        if not declFilter or declFilter(decl) then
            if decl.class == 'Function' then
                if not decl.name:find('operator') then
                    table.insert(funcs, decl)
                end
            elseif decl.name and #decl.name > 0 then
                table.insert(types, decl)
            else
                if CSDecl(f, decl, option, i) then
                    hasComInterface = true
                end
            end
        end
    end
    for i, decl in ipairs(types) do
        if CSDecl(f, decl, option, i, sourceDir) then
            hasComInterface = true
        end
    end

    -- funcs
    if #funcs > 0 then
        writefln(f, '    public static class %s {', source.name)
        for i, decl in ipairs(funcs) do
            CSGlobalFunction(f, decl, '        ', option, source.name)
        end
        writeln(f, '    }')
    end

    writeln(f, '}')

    io.close(f)
    return hasComInterface
end

local function ComUtil(option)
    local path = string.format('%s/ComUtil.cs', option.dir)
    local f = io.open(path, 'w')
    writeln(f, HEADLINE)
    f:write(
        string.format(
            [[
using System.Text;

namespace %s
{
    public static class ComPtrExtensions
    {
        public static T QueryInterface<T>(this IUnknown self) where T : ComPtr, new()
        {
            var p = new T();
            if (self.QueryInterface(ref p.GetIID(), out p.PtrForNew).Failed())
            {
                return null;
            }
            return p;
        }
    }

    /// <summary>
    /// COMの virtual function table を自前で呼び出すヘルパークラス。
    /// </summary>
    public abstract class ComPtr : IDisposable
    {
        static Guid s_uuid;
        public static ref Guid IID => ref s_uuid;
        public virtual ref Guid GetIID(){ return ref s_uuid; }
 
        /// <summay>
        /// IUnknown を継承した interface(ID3D11Deviceなど) に対するポインター。
        /// このポインターの指す領域の先頭に virtual function table へのポインタが格納されている。
        /// <summay>
        protected IntPtr m_ptr;

        public ref IntPtr PtrForNew
        {
            get
            {
                if (m_ptr != IntPtr.Zero)
                {
                    Marshal.Release(m_ptr);
                }
                return ref m_ptr;
            }
        }

        public ref IntPtr Ptr => ref m_ptr;

        public static implicit operator bool(ComPtr i)
        {
            if (i is null) return false;
            return i.m_ptr != IntPtr.Zero;
        }

        IntPtr VTable => Marshal.ReadIntPtr(m_ptr);

        static readonly int IntPtrSize = Marshal.SizeOf(typeof(IntPtr));

        protected IntPtr GetFunctionPointer(int index)
        {
            return Marshal.ReadIntPtr(VTable, index * IntPtrSize);
        }

        #region IDisposable Support
        private bool disposedValue = false; // 重複する呼び出しを検出するには

        protected virtual void Dispose(bool disposing)
        {
            if (!disposedValue)
            {
                if (disposing)
                {
                    // TODO: マネージ状態を破棄します (マネージ オブジェクト)。
                }

                // TODO: アンマネージ リソース (アンマネージ オブジェクト) を解放し、下のファイナライザーをオーバーライドします。
                // TODO: 大きなフィールドを null に設定します。
                if (m_ptr != IntPtr.Zero)
                {
                    Marshal.Release(m_ptr);
                    m_ptr = IntPtr.Zero;
                }

                disposedValue = true;
            }
        }

        ~ComPtr()
        {
            // このコードを変更しないでください。クリーンアップ コードを上の Dispose(bool disposing) に記述します。
            Dispose(false);
        }

        // このコードは、破棄可能なパターンを正しく実装できるように追加されました。
        public void Dispose()
        {
            // このコードを変更しないでください。クリーンアップ コードを上の Dispose(bool disposing) に記述します。
            Dispose(true);
            // TODO: 上のファイナライザーがオーバーライドされる場合は、次の行のコメントを解除してください。
            // GC.SuppressFinalize(this);
        }
        #endregion
    }

    class CustomMarshaler<T> : ICustomMarshaler
    where T : ComPtr, new()
    {
        public void CleanUpManagedData(object ManagedObj)
        {
            throw new NotImplementedException();
        }

        public void CleanUpNativeData(IntPtr pNativeData)
        {
            throw new NotImplementedException();
        }

        public int GetNativeDataSize()
        {
            throw new NotImplementedException();
        }

        public IntPtr MarshalManagedToNative(object ManagedObj)
        {
            throw new NotImplementedException();
        }

        public object MarshalNativeToManaged(IntPtr pNativeData)
        {
            // var count = Marshal.AddRef(pNativeData);
            // Marshal.Release(pNativeData);
            var t = new T();
            t.PtrForNew = pNativeData;
            return t;
        }

        public static ICustomMarshaler GetInstance(string src)
        {
            return new CustomMarshaler<T>();
        }
    }

    public static class WindowsAPIExtensions
    {
        public static short HIWORD(this ulong _dw)
        {
            return ((short)((_dw >> 16) & 0xffff));
        }

        public static short LOWORD(this ulong _dw)
        {
            return ((short)(_dw & 0xffff));
        }

        public static short HIWORD(this long _dw)
        {
            return ((short)((_dw >> 16) & 0xffff));
        }

        public static short LOWORD(this long _dw)
        {
            return ((short)(_dw & 0xffff));
        }

        [ThreadStatic]
        static IntPtr ts_ptr = Marshal.AllocHGlobal(4);

        public static IntPtr Ptr(this int value)
        {
            Marshal.WriteInt32(ts_ptr, value);
            return ts_ptr;
        }

        public static IntPtr Ptr(this string src)
        {
            IntPtr strPtr = Marshal.StringToHGlobalUni(src);
            try
            {
                return strPtr;
            }
            finally
            {
                Marshal.FreeHGlobal(strPtr);
            }
        }

        public static void ThrowIfFailed(this int hr)
        {
            if (hr != 0)
            {
                var ex = new ComException(hr);
                throw ex;
            }
        }

        public static bool Succeeded(this int hr) => hr == 0;

        public static bool Failed(this int hr) => hr != 0;

        public static WSTR ToMutableString(this ushort[] src)
        {
            return new WSTR(src);
        }
    }

    public class ComException : Exception
    {
        public readonly int HR;
 
        public ComException(int hr)
        {
            HR = hr;
        }
    }

    public struct WSTR
    {
        // zero terminated
        public byte[] Buffer;

        public int Length => Buffer.Length-2;

        public ref ushort Data
        {
            get
            {
                return ref MemoryMarshal.Cast<byte, ushort>(Buffer.AsSpan())[0];
            }
        }

        public WSTR(string src)
        {
            Buffer = Encoding.Unicode.GetBytes(src + "\0");
        }

        public WSTR(ushort[] src)
        {
            var end = Array.IndexOf<ushort>(src, 0, 0);
            if (end == -1)
            {
                end = src.Length;
            }
            var span = MemoryMarshal.Cast<ushort, byte>(src.AsSpan().Slice(0, end));
            Buffer = new byte[end * 2 + 2];
            span.CopyTo(Buffer.AsSpan());
        }

        public override string ToString()
        {
            return Encoding.Unicode.GetString(Buffer, 0, Buffer.Length - 2);
        }
    }

    public struct STR
    {
        // zero terminated
        public byte[] Buffer;

        static readonly Encoding s_utf8 = new UTF8Encoding(false);

        public STR(string src)
        {
            Buffer = s_utf8.GetBytes(src + "\0");
        }

        public STR(byte[] src)
        {
            var end = Array.IndexOf<byte>(src, 0, 0);
            if (end == -1)
            {
                end = src.Length;
            }
            Buffer = new byte[end * 2 + 2];
            src.AsSpan().CopyTo(Buffer.AsSpan());
        }

        public override string ToString()
        {
            return s_utf8.GetString(Buffer, 0, Buffer.Length - 2);
        }
    }    
}
]],
            option.packageName
        )
    )
    io.close(f)
end

local function CSProj(f)
    f:write(
        [[
<Project Sdk="Microsoft.NET.Sdk">

  <PropertyGroup>
    <TargetFramework>netstandard2.0</TargetFramework>
  </PropertyGroup>

  <ItemGroup>
    <PackageReference Include="System.Memory" Version="4.5.3" />
  </ItemGroup>

</Project>
    ]]
    )
end

local function CSGenerate(sourceMap, option)
    -- clear dir
    if file.exists(option.dir) then
        printf('rmdir %s', option.dir)
        file.rmdirRecurse(option.dir)
    end
    option.packageName = basename(option.dir)
    file.mkdirRecurse(option.dir)

    local hasComInterface = false
    for k, source in pairs(sourceMap) do
        -- write each source
        if not source.empty then
            if CSSource(f, source, option) then
                hasComInterface = true
            end
        end
    end

    if hasComInterface then
        -- write utility
        ComUtil(option)
    end

    do
        -- csproj
        local path = string.format('%s/%s.csproj', option.dir, option.packageName)
        local f = io.open(path, 'w')
        CSProj(f)
        io.close(f)
    end
end

return {
    Generate = CSGenerate
}
