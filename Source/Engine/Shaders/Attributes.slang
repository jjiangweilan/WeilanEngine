[__AttributeUsage(_AttributeTargets.Var)]
struct FilterModeAttribute
{
    string filter;
};

[__AttributeUsage(_AttributeTargets.Var)]
struct ByteSizeAttribute
{
    int filter;
};

[__AttributeUsage(_AttributeTargets.Var)]
struct UnderlyingFormatAttribute
{
    string format;
};
