#include "shader_Common.hlsli"
#include "shader_FragmentList_HQ.hlsli"


StructuredBuffer<FragmentLink> FragmentLinkSRV : register(t0);


struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float Value : VALUE;
};

 
// pos between (-1, 1) -> why are -1 and 1 not included
// -1.f + 1.f / HISTOGRAM_BINS -> first bin
// -1.f + 3.f / HISTOGRAM_BINS -> second bin
// -1.f + 5.f / HISTOGRAM_BINS -> third bin
// and so on
PS_INPUT VS(uint vertexID : SV_VertexID)
{   
    FragmentLink data = FragmentLinkSRV[vertexID];
    float delta = 1.f / HISTOGRAM_BINS;
    float bin = floor(data.fragmentData.ScalarColor * (HISTOGRAM_BINS - 1.f));
    float pos = -1.f + (2 * bin + 1.f) * delta;

    PS_INPUT output;
    output.Position = float4(pos, 0.f, 0.0f, 1.f);
    output.Value = data.fragmentData.Transmittance;
    return output;
}

float PS(PS_INPUT input) : SV_Target0
{
    return input.Value;
}