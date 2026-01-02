---@meta
-- GENERATED FILE - DO NOT EDIT
-- This file provides LuaLS annotations for WeilanEngine C++ bindings.

---@class wl
wl = {}

---@class wl.Gamepad
wl.Gamepad = {}

function wl.Gamepad:IsButtonPressed(...) end
function wl.Gamepad:IsBumperPressed(...) end
function wl.Gamepad:GetTrigger(...) end
function wl.Gamepad:GetAxis(...) end

---@class wl.Input
wl.Input = {}

function wl.Input.GetGamepad(...) end
function wl.Input.GetMovementX(...) end
function wl.Input.GetMovementY(...) end
function wl.Input.IsInteractPressed(...) end
function wl.Input.GetLookAroundX(...) end
function wl.Input.GetLookAroundY(...) end
function wl.Input.Jump(...) end

---@class wl.AnimationPlayer
wl.AnimationPlayer = {}

function wl.AnimationPlayer:SetClip(...) end
function wl.AnimationPlayer:Play(...) end

---@class wl.GameScript
wl.GameScript = {}

function wl.GameScript:GetGameObject(...) end
function wl.GameScript.New(...) end

---@class wl.GameObject
wl.GameObject = {}

function wl.GameObject:GetPosition(...) end
function wl.GameObject:SetPosition(...) end
function wl.GameObject:GetComponentInHierachy(...) end
function wl.GameObject:LookAt(...) end
function wl.GameObject:GetComponent(...) end

---@class wl.Time
wl.Time = {}

function wl.Time.DeltaTime(...) end

---@class wl.Float2
---@field x any
---@field y any
wl.Float2 = {}

function wl.Float2.New(...) end
function wl.Float2.Dot(...) end
function wl.Float2.__eq(...) end
function wl.Float2.__add(...) end
function wl.Float2.__sub(...) end
function wl.Float2.__div(...) end
function wl.Float2.__mul(...) end
function wl.Float2:GetX(...) end
function wl.Float2:GetY(...) end
function wl.Float2:SetX(...) end
function wl.Float2:SetY(...) end

---@class wl.Float3
---@field x any
---@field y any
---@field z any
wl.Float3 = {}

function wl.Float3.New(...) end
function wl.Float3.Dot(...) end
function wl.Float3.__eq(...) end
function wl.Float3.__add(...) end
function wl.Float3.__sub(...) end
function wl.Float3.__div(...) end
function wl.Float3.__mul(...) end
function wl.Float3:GetX(...) end
function wl.Float3:GetY(...) end
function wl.Float3:GetZ(...) end
function wl.Float3:SetX(...) end
function wl.Float3:SetY(...) end
function wl.Float3:SetZ(...) end

---@class wl.Float4
---@field x any
---@field y any
---@field z any
---@field w any
wl.Float4 = {}

function wl.Float4.New(...) end
function wl.Float4.Dot(...) end
function wl.Float4.__eq(...) end
function wl.Float4.__add(...) end
function wl.Float4.__sub(...) end
function wl.Float4.__div(...) end
function wl.Float4.__mul(...) end
function wl.Float4:GetX(...) end
function wl.Float4:GetY(...) end
function wl.Float4:GetZ(...) end
function wl.Float4:GetW(...) end
function wl.Float4:SetX(...) end
function wl.Float4:SetY(...) end
function wl.Float4:SetZ(...) end
function wl.Float4:SetW(...) end

---@class wl.Camera
wl.Camera = {}

function wl.Camera:LookAt(...) end

---@class wl.Light
wl.Light = {}

function wl.Light:GetIntensity(...) end
function wl.Light:SetIntensity(...) end

---@class wl.MeshRenderer
wl.MeshRenderer = {}

function wl.MeshRenderer:SetMaterial(...) end

---@class wl.ObjPtr
wl.ObjPtr = {}

function wl.ObjPtr.New(...) end
function wl.ObjPtr.IsValid(...) end

---@class wl.Texture
wl.Texture = {}


---@class wl.Material
wl.Material = {}

function wl.Material:SetTexture(...) end
function wl.Material:SetShader(...) end
function wl.Material:GetTexture(...) end
function wl.Material:GetShader(...) end
function wl.Material:SetFloat(...) end
function wl.Material:SetVector(...) end
function wl.Material:SetName(...) end
function wl.Material:GetName(...) end
