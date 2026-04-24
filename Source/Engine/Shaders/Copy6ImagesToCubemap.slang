struct Parameter
{
    Texture2D faces[6];
};

ParameterBlock<Parameter> input;

[shader("compute")]
[numthreads(8,8,1)]
void Main(uint id : SV_GroupThreadID)
{

}
