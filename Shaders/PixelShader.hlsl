
struct PS_Input
{
    float4 vertex : SV_POSITION;
    float3 color  : COLOR;
};

struct PS_Output
{
    float4 color : SV_TARGET;
};

PS_Output main(PS_Input input)
{
    PS_Output Output;

    Output.color.rgb = input.color;
    Output.color.a = 1;
    return Output;
}
