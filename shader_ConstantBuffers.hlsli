#include "shader_Common.hlsli"


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
    float dt;
    float DampingHalflife;
    int HistogramMode;
    float paddingForTheArray;
    float4 HistogramSegments[HISTOGRAM_SEGMENTS_FLOAT4_MAX];
}