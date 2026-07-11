[__AttributeUsage(_AttributeTargets.Var)]
struct GlobalAttribute
{
};

[__AttributeUsage(_AttributeTargets.Var)]
struct MaterialAttribute
{
};

[__AttributeUsage(_AttributeTargets.Var)]
struct PassAttribute
{
};

[__AttributeUsage(_AttributeTargets.Var)]
struct ObjectAttribute
{
};

[__AttributeUsage(_AttributeTargets.Var)]
struct ColorAttribute
{
};

[__AttributeUsage(_AttributeTargets.Var)]
struct TextureAttribute
{
};

[__AttributeUsage(_AttributeTargets.Struct)]
struct MaterialUILayoutAttribute
{
};

[__AttributeUsage(_AttributeTargets.Var)]
struct GroupAttribute
{
    string name;
};

[__AttributeUsage(_AttributeTargets.Var)]
struct RangeAttribute
{
    float min;
    float max;
};

[__AttributeUsage(_AttributeTargets.Var)]
struct TooltipAttribute
{
    string text;
};
