---@meta
-- GENERATED FILE - DO NOT EDIT
-- This file provides LuaLS annotations for WeilanEngine C++ bindings.

---@class wl
wl = {}

---@class wl.Gamepad
local Gamepad = {}
wl.Gamepad = Gamepad

function Gamepad:IsButtonPressed(...) end
function Gamepad:IsBumperPressed(...) end
function Gamepad:GetTrigger(...) end
function Gamepad:GetAxis(...) end

---@class wl.Input
local Input = {}
wl.Input = Input

function Input.GetGamepad(...) end
function Input.GetMovementX(...) end
function Input.GetMovementY(...) end
function Input.IsInteractPressed(...) end
function Input.GetLookAroundX(...) end
function Input.GetLookAroundY(...) end
function Input.Jump(...) end

---@class wl.AnimationPlayer
local AnimationPlayer = {}
wl.AnimationPlayer = AnimationPlayer

function AnimationPlayer:SetClip(...) end
function AnimationPlayer:Play(...) end

---@class wl.GameScript
local GameScript = {}
wl.GameScript = GameScript

function GameScript:GetGameObject(...) end
function GameScript.New(...) end

---@class wl.GameObject
local GameObject = {}
wl.GameObject = GameObject

function GameObject:GetPosition(...) end
function GameObject:SetPosition(...) end
function GameObject:GetComponentInHierachy(...) end
function GameObject:LookAt(...) end
function GameObject:GetComponent(...) end

---@class wl.Time
local Time = {}
wl.Time = Time

function Time.DeltaTime(...) end

---@class wl.Float2
---@field x any
---@field y any
local Float2 = {}
wl.Float2 = Float2

function Float2.New(...) end
function Float2.Dot(...) end
function Float2.__eq(...) end
function Float2.__add(...) end
function Float2.__sub(...) end
function Float2.__div(...) end
function Float2.__mul(...) end
function Float2:GetX(...) end
function Float2:GetY(...) end
function Float2:SetX(...) end
function Float2:SetY(...) end

---@class wl.Float3
---@field x any
---@field y any
---@field z any
local Float3 = {}
wl.Float3 = Float3

function Float3.New(...) end
function Float3.Dot(...) end
function Float3.__eq(...) end
function Float3.__add(...) end
function Float3.__sub(...) end
function Float3.__div(...) end
function Float3.__mul(...) end
function Float3:GetX(...) end
function Float3:GetY(...) end
function Float3:GetZ(...) end
function Float3:SetX(...) end
function Float3:SetY(...) end
function Float3:SetZ(...) end

---@class wl.Float4
---@field x any
---@field y any
---@field z any
---@field w any
local Float4 = {}
wl.Float4 = Float4

function Float4.New(...) end
function Float4.Dot(...) end
function Float4.__eq(...) end
function Float4.__add(...) end
function Float4.__sub(...) end
function Float4.__div(...) end
function Float4.__mul(...) end
function Float4:GetX(...) end
function Float4:GetY(...) end
function Float4:GetZ(...) end
function Float4:GetW(...) end
function Float4:SetX(...) end
function Float4:SetY(...) end
function Float4:SetZ(...) end
function Float4:SetW(...) end

---@class wl.Camera
local Camera = {}
wl.Camera = Camera

function Camera:LookAt(...) end

---@class wl.Light
local Light = {}
wl.Light = Light

function Light:GetIntensity(...) end
function Light:SetIntensity(...) end

---@class wl.MeshRenderer
local MeshRenderer = {}
wl.MeshRenderer = MeshRenderer

function MeshRenderer:SetMaterial(...) end

---@class wl.ObjPtr
local ObjPtr = {}
wl.ObjPtr = ObjPtr

function ObjPtr.New(...) end
function ObjPtr.IsValid(...) end

---@class wl.Texture
local Texture = {}
wl.Texture = Texture


---@class wl.Material
local Material = {}
wl.Material = Material

function Material:SetTexture(...) end
function Material:SetShader(...) end
function Material:GetTexture(...) end
function Material:GetShader(...) end
function Material:SetFloat(...) end
function Material:SetVector(...) end
function Material:SetName(...) end
function Material:GetName(...) end
