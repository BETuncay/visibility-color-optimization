#include "shader_Common.hlsli"
#include "shader_FragmentList_HQ.hlsli"


struct HistogramData
{
    float scalarColor;
    float transmittance;
};

cbuffer CameraParameters : register(b0)
{
    row_major matrix View = matrix(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
    row_major matrix Projection = matrix(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
}

cbuffer RendererParameters : register(b1)
{
    float Q;
    float R;
    float Lambda;
    int TotalNumberOfControlPoints;
    float4 LineColor;
    float4 HaloColor;
    float StripWidth;
    float HaloPortion;
    int ScreenWidth;
    int ScreenHeight;
}

RWStructuredBuffer<FragmentLink> FragmentLinkSRV :  register(u1);
RWByteAddressBuffer StartOffsetSRV :                register(u2);
RWByteAddressBuffer HistogramBuffer :               register(u3);


HistogramData HLoad(uint address)
{
    uint status;
    uint2 temp = HistogramBuffer.Load2(address * 8, status);
    HistogramData element;
    element.scalarColor = asfloat(temp[0]);
    element.transmittance = asfloat(temp[1]);
    return element;
}

float4 VS(float4 pos : POSITION) : SV_POSITION
{
    return pos;
}

float4 PS(float4 input : SV_Position) : SV_Target0
{
    uint nIndex = (uint) input.y * ScreenWidth + (uint) input.x;
    uint nFirst = StartOffsetSRV.Load(nIndex * 4); 
    //uint hFirst = HistogramBuffer.Load(nIndex * 4);
   
    
    if (nFirst == 0xFFFFFFFF)
    {
        return float4(0, 0, 0, 1);
    }
    
    FragmentLink element = FragmentLinkSRV[nFirst];

    
    if (element.fragmentData.ScalarColor == nFirst)
    {
        return float4(0, 1, 0, 1);
    }
    else
    {
        return float4(1, 0, 0, 1);
    }
}