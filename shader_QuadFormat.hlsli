struct QuadVSinput
{
    float4 pos : POSITION;
};

struct QuadVS_Output
{
    float4 pos : SV_POSITION;
};

struct QuadPS_Input
{
    float4 pos : SV_POSITION;
};