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
    local tsm = self.gameObject:GetComponent("Transform")
    tsm:SetPosition({3,2,1})
    GlobalReadbackTestTick = self.gameObject:GetName()
end
