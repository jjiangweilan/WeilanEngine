---@meta
-- GENERATED FILE - DO NOT EDIT
-- This file provides LuaLS annotations for WeilanEngine C++ bindings.

---@class wl
wl = {}

---@class wl.Gamepad
wl.Gamepad = {}

---@param idx number
---@return boolean
function wl.Gamepad:IsButtonPressed(idx) end
---@param idx number
---@return boolean
function wl.Gamepad:IsBumperPressed(idx) end
---@param idx number
---@return number
function wl.Gamepad:GetTrigger(idx) end
---@param idx number
---@return wl.Float2
function wl.Gamepad:GetAxis(idx) end

---@class wl.Input
wl.Input = {}

---@return wl.Gamepad
function wl.Input.GetGamepad(...) end
---@return number
function wl.Input.GetMovementX(...) end
---@return number
function wl.Input.GetMovementY(...) end
---@return boolean
function wl.Input.IsInteractPressed(...) end
---@return number
function wl.Input.GetLookAroundX(...) end
---@return number
function wl.Input.GetLookAroundY(...) end
---@return boolean
function wl.Input.Jump(...) end

---@class wl.AnimationPlayer
wl.AnimationPlayer = {}

---@param animationName string
---@return boolean
function wl.AnimationPlayer:SetClip(animationName) end
function wl.AnimationPlayer:Play(...) end

---@class wl.GameObject
wl.GameObject = {}

---@param className any
---@return wl.Component
function wl.GameObject:GetComponentInHierachy(className) end
---@return wl.Float3
function wl.GameObject:GetPosition(...) end
---@param position wl.vec3 &
function wl.GameObject:SetPosition(position) end
---@param to wl.vec3 &
function wl.GameObject:LookAt(to) end
function wl.GameObject:GetComponent(...) end

---@class wl.GameScript
wl.GameScript = {}

function wl.GameScript:GetGameObject(...) end
function wl.GameScript.New(...) end

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
---@return wl.string&, float)>
function wl.Material:SetFloat(...) end
---@return wl.vec4&)>
function wl.Material:SetVector(...) end
function wl.Material:SetName(...) end
function wl.Material:GetName(...) end
