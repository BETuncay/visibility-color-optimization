#include "shader_Common.hlsli"
#include "shader_ConstantBuffers.hlsli"
#include "shader_FragmentList_HQ.hlsli"
#include "shader_QuadFormat.hlsli"


RWByteAddressBuffer StartOffsetSRV : register(u1);
RWStructuredBuffer<FragmentLink> FragmentLinkSRV : register(u2);

StructuredBuffer<float> HistogramCDF : register(t0);
Texture2D<float3> Colormap : register(t1);
Texture2D<float> Histogram : register(t2);

SamplerState samplerState : register(s0);


QuadVS_Output VS(QuadVSinput Input)
{
    QuadVS_Output Output;
    Output.pos = Input.pos;
    return Output;
}

// put in header
float normalize_scalarColor(float scalarColor)
{
    // bilinear interpolate between indices
    float index = scalarColor * (HISTOGRAM_BINS - 1.f);
    uint idx = floor(index);
    float fract = index - idx;
        
    float value = 0;
    value += HistogramCDF[index] * (1 - fract);
    value += HistogramCDF[min(index + 1, HISTOGRAM_BINS - 1)] * fract;
    value = (value - HistogramCDF[0]) / (1.f - HistogramCDF[0]);
    return value;
}

float normalize_bi_scalarColor(float scalarColor, uint medianIndex)
{
    if (medianIndex == 0 || medianIndex == HISTOGRAM_BINS - 1)
    {
        return normalize_scalarColor(scalarColor);
    }
    // bilinear interpolate between indices
    float index = scalarColor * (HISTOGRAM_BINS - 1.f);
    uint idx = floor(index);
    float fract = index - idx;
     
    // handle bi histogram switching between the halfs    
    float value = 0;
    if (idx <= medianIndex)
    {
        // transform function lower: f_L(x) = X_0 + (X_m - X_0) * c_L(x)
        float median = ((float) medianIndex) / (HISTOGRAM_BINS - 1.f);
        value += median * HistogramCDF[idx] * (1 - fract);
        value += median * HistogramCDF[min(idx + 1, medianIndex)] * fract;
        //value = (value - HistogramCDF[0]) / (median - HistogramCDF[0]);
    }
    else
    {
        // transform function upper: f_U(x) = X_m + (1 - X_m) * c_U(x)
        float median_plus_one = ((float) min(medianIndex + 1, HISTOGRAM_BINS - 1)) / (HISTOGRAM_BINS - 1.f);
        value += median_plus_one + (1.f - median_plus_one) * HistogramCDF[idx] * (1 - fract);
        value += median_plus_one + (1.f - median_plus_one) * HistogramCDF[min(idx + 1, HISTOGRAM_BINS - 1)] * fract;
        //value = (value - median_plus_one) / (1.f - median_plus_one);
    }
    //value = (value - HistogramCDF[0]) / (1.f - HistogramCDF[0]);
    return value;
}

// calculate mean index
uint calculate_mean_idx()
{
    float sum = 0;
    float median = 0;
    for (uint i = 0; i < HISTOGRAM_BINS; i++)
    {
        sum += Histogram.Load(int3(i, 0, 0));
        median += Histogram.Load(int3(i, 0, 0)) * i;
    }
    median /= sum;
    return (uint) median;
}

float normalize_segmented_scalarColor(float scalarColor)
{
    float segmentLower = 0;
    float segmentUpper = 1;
    
    // find segment boundaries
    for (uint i = 1; i < HISTOGRAM_SEGMENTS_MAX; i++)
    {
        if (scalarColor <= HistogramSegments[i >> 2][i & 3])
        {
            segmentLower = HistogramSegments[(i - 1) >> 2][(i - 1) & 3];
            segmentUpper = HistogramSegments[i >> 2][i & 3];
            break;
        }
    }
    
    // transform function: f_S(x) = S_l + (S_h -  S_l) * c_S(x)
    float index = scalarColor * (HISTOGRAM_BINS - 1.f);
    uint idx = floor(index);
    float fract = index - idx;
    uint segmentUpperIndex = floor(segmentUpper * (HISTOGRAM_BINS - 1.f));

    float value = (segmentLower + (segmentUpper - segmentLower) * HistogramCDF[idx]) * (1 - fract);
    value += (segmentLower + (segmentUpper - segmentLower) * HistogramCDF[min(idx + 1, segmentUpperIndex)]) * fract;
    return value;
}


float4 PS(QuadPS_Input input) : SV_Target0
{
    uint nIndex = (uint) input.pos.y * ScreenWidth + (uint) input.pos.x; // index to current pixel.
    uint nNext = StartOffsetSRV.Load(nIndex * 4); // get first fragment from the start offset buffer.
    
    // early exit if no fragments in the linked list.
    if (nNext == 0xFFFFFFFF)
    {
        return float4(0.f, 0.f, 0.f, 0.f);
    }
    
    float4 result = float4(0.f, 0.f, 0.f, 1.f);
    float allAlpha = 1;
    uint histogramMeanIdx;
    if (HistogramMode == 2)
    {
        histogramMeanIdx = calculate_mean_idx();
    }

    // Iterate the list and blend
	[allow_uav_condition]
    for (int i = 0; i < TEMPORARY_BUFFER_MAX; i++)
    {
        if (nNext == 0xFFFFFFFF)
            break;
        
        FragmentLink element = FragmentLinkSRV[nNext];
        nNext = element.nNext;
        
        float value = element.fragmentData.ScalarColor;
        if (HistogramMode == 1)
        {
            value = normalize_scalarColor(value);
        }
        if (HistogramMode == 2)
        {
            value = normalize_bi_scalarColor(value, histogramMeanIdx);
        }
        if (HistogramMode == 3)
        {
            value = value; // dont apply histogram equalization
        }
        if (HistogramMode == 4)
        {
            value = normalize_segmented_scalarColor(value); // segmented Histogram equalization
        }

        float4 color;
        if (HistogramMode == 0) // ignore scalarColor and just use color in constant buffer
        {
            color = LineColor;
            color.a = element.fragmentData.Transparency;
        }
        else
        {
            color = float4(Colormap.SampleLevel(samplerState, float2(value, 0.f), 0.f), element.fragmentData.Transparency);
        }
        
        if (element.fragmentData.halfDistCenter * 2 > HaloPortion)
        {
            color = lerp(color, float4(HaloColor.xyz, element.fragmentData.Transparency), element.fragmentData.halfDistCenter);
        }
        
        float fDiff = ((element.fragmentData.fDifffSpec >> 0) & 0xFFFF) / 511.0f;
        float fSpec = ((element.fragmentData.fDifffSpec >> 16) & 0xFFFF) / 511.0f;
        color = float4(saturate((0.2 + 0.6 * fDiff) * color.xyz + 0.4 * fSpec.xxx), color.a);
        
        result.rgb += color.rgb * (1 - color.a) * allAlpha;
        allAlpha *= (color.a);
    }
    
    result.a = 1 - allAlpha;
	// final composition will do alpha blending like so:
	// result.rgb * (result.a) + background * (1-result.a)
	// since we already weighted with the correct alpha, we divide once away, so that the blend state will cancel it out.
	// would be a little nicer to change the blend state
    result.rgb /= result.a;
    return result;
}