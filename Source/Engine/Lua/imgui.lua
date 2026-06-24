local ffi = require("ffi")

ffi.cdef[[
bool WeilanImGui_Begin(const char* name, bool* open, int flags);
void WeilanImGui_End(void);
void WeilanImGui_Text(const char* text);
bool WeilanImGui_Button(const char* label, float width, float height);
bool WeilanImGui_Checkbox(const char* label, bool* value);
bool WeilanImGui_SliderFloat(const char* label, float* value, float minValue, float maxValue);
bool WeilanImGui_DragFloat3(const char* label, float* values, float speed, float minValue, float maxValue);
bool WeilanImGui_InputText(const char* label, char* buffer, size_t bufferSize, int flags);
void WeilanImGui_SameLine(float offsetFromStartX, float spacing);
void WeilanImGui_Separator(void);
void WeilanImGui_NewLine(void);
void WeilanImGui_Spacing(void);
void WeilanImGui_SetNextWindowSize(float width, float height, int condition);
void WeilanImGui_SetNextWindowPos(float x, float y, int condition);
bool WeilanImGui_CollapsingHeader(const char* label, int flags);
bool WeilanImGui_TreeNode(const char* label);
void WeilanImGui_TreePop(void);
void WeilanImGui_GetContentRegionAvail(float* width, float* height);
bool WeilanImGui_IsWindowHovered(int flags);
bool WeilanImGui_IsItemHovered(int flags);
]]

local function load_engine()
    local candidates
    if jit.os == "Windows" then
        candidates = { "WeilanEngine" }
    elseif jit.os == "OSX" then
        candidates = { "WeilanEngine", "libWeilanEngine.dylib" }
    else
        candidates = { "WeilanEngine", "libWeilanEngine.so" }
    end

    local last_error
    for _, name in ipairs(candidates) do
        local ok, lib = pcall(ffi.load, name)
        if ok then
            return lib
        end
        last_error = lib
    end

    error("failed to load WeilanEngine ImGui symbols: " .. tostring(last_error))
end

local lib = load_engine()
local M = {}

M.Cond_None = 0
M.Cond_Always = 1
M.Cond_Once = 2
M.Cond_FirstUseEver = 4
M.Cond_Appearing = 8

M.WindowFlags_None = 0
M.WindowFlags_NoTitleBar = 1
M.WindowFlags_NoResize = 2
M.WindowFlags_NoMove = 4
M.WindowFlags_NoScrollbar = 8
M.WindowFlags_NoScrollWithMouse = 16
M.WindowFlags_NoCollapse = 32
M.WindowFlags_AlwaysAutoResize = 64
M.WindowFlags_NoBackground = 128
M.WindowFlags_NoSavedSettings = 256
M.WindowFlags_NoMouseInputs = 512
M.WindowFlags_MenuBar = 1024
M.WindowFlags_HorizontalScrollbar = 2048
M.WindowFlags_NoFocusOnAppearing = 4096
M.WindowFlags_NoBringToFrontOnFocus = 8192
M.WindowFlags_AlwaysVerticalScrollbar = 16384
M.WindowFlags_AlwaysHorizontalScrollbar = 32768
M.WindowFlags_NoNavInputs = 65536
M.WindowFlags_NoNavFocus = 131072
M.WindowFlags_UnsavedDocument = 1048576

M.InputTextFlags_None = 0
M.InputTextFlags_CharsDecimal = 1
M.InputTextFlags_CharsHexadecimal = 2
M.InputTextFlags_CharsUppercase = 4
M.InputTextFlags_CharsNoBlank = 8
M.InputTextFlags_AutoSelectAll = 16
M.InputTextFlags_EnterReturnsTrue = 32
M.InputTextFlags_CallbackCompletion = 64
M.InputTextFlags_CallbackHistory = 128
M.InputTextFlags_CallbackAlways = 256
M.InputTextFlags_CallbackCharFilter = 512
M.InputTextFlags_AllowTabInput = 1024
M.InputTextFlags_CtrlEnterForNewLine = 2048
M.InputTextFlags_NoHorizontalScroll = 4096
M.InputTextFlags_AlwaysOverwrite = 8192
M.InputTextFlags_ReadOnly = 16384
M.InputTextFlags_Password = 32768
M.InputTextFlags_NoUndoRedo = 65536
M.InputTextFlags_CharsScientific = 131072
M.InputTextFlags_CallbackResize = 262144
M.InputTextFlags_CallbackEdit = 524288
M.InputTextFlags_EscapeClearsAll = 1048576

function M.Begin(name, open, flags)
    local open_ptr = nil
    if open ~= nil then
        open_ptr = ffi.new("bool[1]", open)
    end

    local visible = lib.WeilanImGui_Begin(name, open_ptr, flags or 0)
    if open_ptr ~= nil then
        return visible, open_ptr[0]
    end
    return visible
end

function M.End()
    lib.WeilanImGui_End()
end

function M.Text(text, ...)
    if select("#", ...) > 0 then
        text = string.format(text, ...)
    end
    lib.WeilanImGui_Text(tostring(text))
end

function M.Button(label, width, height)
    return lib.WeilanImGui_Button(label, width or 0, height or 0)
end

function M.Checkbox(label, value)
    local value_ptr = ffi.new("bool[1]", value and true or false)
    local changed = lib.WeilanImGui_Checkbox(label, value_ptr)
    return changed, value_ptr[0]
end

function M.SliderFloat(label, value, min_value, max_value)
    local value_ptr = ffi.new("float[1]", value or 0)
    local changed = lib.WeilanImGui_SliderFloat(label, value_ptr, min_value, max_value)
    return changed, value_ptr[0]
end

function M.DragFloat3(label, values, speed, min_value, max_value)
    local value_ptr = ffi.new("float[3]", { values[1] or 0, values[2] or 0, values[3] or 0 })
    local changed = lib.WeilanImGui_DragFloat3(label, value_ptr, speed or 1.0, min_value or 0.0, max_value or 0.0)
    return changed, { value_ptr[0], value_ptr[1], value_ptr[2] }
end

function M.InputText(label, value, capacity, flags)
    capacity = capacity or 256
    local buffer = ffi.new("char[?]", capacity)
    local text = tostring(value or "")
    if #text >= capacity then
        text = string.sub(text, 1, capacity - 1)
    end
    ffi.copy(buffer, text)
    local changed = lib.WeilanImGui_InputText(label, buffer, capacity, flags or 0)
    return changed, ffi.string(buffer)
end

function M.SameLine(offset_from_start_x, spacing)
    lib.WeilanImGui_SameLine(offset_from_start_x or 0, spacing or -1)
end

function M.Separator()
    lib.WeilanImGui_Separator()
end

function M.NewLine()
    lib.WeilanImGui_NewLine()
end

function M.Spacing()
    lib.WeilanImGui_Spacing()
end

function M.SetNextWindowSize(width, height, condition)
    lib.WeilanImGui_SetNextWindowSize(width, height, condition or 0)
end

function M.SetNextWindowPos(x, y, condition)
    lib.WeilanImGui_SetNextWindowPos(x, y, condition or 0)
end

function M.CollapsingHeader(label, flags)
    return lib.WeilanImGui_CollapsingHeader(label, flags or 0)
end

function M.TreeNode(label)
    return lib.WeilanImGui_TreeNode(label)
end

function M.TreePop()
    lib.WeilanImGui_TreePop()
end

function M.GetContentRegionAvail()
    local width = ffi.new("float[1]")
    local height = ffi.new("float[1]")
    lib.WeilanImGui_GetContentRegionAvail(width, height)
    return width[0], height[0]
end

function M.IsWindowHovered(flags)
    return lib.WeilanImGui_IsWindowHovered(flags or 0)
end

function M.IsItemHovered(flags)
    return lib.WeilanImGui_IsItemHovered(flags or 0)
end

return M
