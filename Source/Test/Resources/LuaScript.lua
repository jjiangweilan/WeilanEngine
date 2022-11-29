ScriptTest = {}

function ScriptTest:New()
    local o = {}
    setmetatable(o, self)
    self.__index = self
    return o
end

function ScriptTest:Construct()
    GlobalReadbackTest = "This should be read in host"
end

function ScriptTest:Destruct()
end

function ScriptTest:Tick()
end
