#include "shader_Common.hlsli"
#include "shader_ConstantBuffers.hlsli"
#include "shader_FragmentList_HQ.hlsli"
#include "shader_QuadFormat.hlsli"


RWByteAddressBuffer StartOffsetSRV                      : register(u1);
RWStructuredBuffer<FragmentLink> FragmentLinkSRV        : register(u2);
StructuredBuffer<float> HistogramCDF                    : register(t0);
Texture2D<float> Histogram                              : register(t1);


// should put this in a header file
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

QuadVS_Output VS(QuadVSinput Input)
{
    QuadVS_Output Output;
    Output.pos = Input.pos;
    return Output;
}

void PS(QuadPS_Input input)
{
	// index to current pixel.
    uint nIndex = (uint) input.pos.y * ScreenWidth + (uint) input.pos.x;
    uint nNext = StartOffsetSRV.Load(nIndex * 4);

	// early exit if no fragments in the linked list.
    if (nNext == 0xFFFFFFFF)
    {
        return;
    }

    uint histogramMeanIdx;
    if (HistogramMode == 2)
    {
        histogramMeanIdx = calculate_mean_idx();
    }
    
	// Normalize scalar values    
    [allow_uav_condition]
    for (int ii = 0; ii < TEMPORARY_BUFFER_MAX; ii++)
    {
        if (nNext == 0xFFFFFFFF)
            break;
        
        FragmentLink element = FragmentLinkSRV[nNext];
        if (HistogramMode == 1)
        {
            element.fragmentData.ScalarColor = normalize_scalarColor(element.fragmentData.ScalarColor);
        }
        if (HistogramMode == 2)
        {
            element.fragmentData.ScalarColor = normalize_bi_scalarColor(element.fragmentData.ScalarColor, histogramMeanIdx);
        }
        if (HistogramMode == 4)
        {
            element.fragmentData.ScalarColor = normalize_segmented_scalarColor(element.fragmentData.ScalarColor);
        }
        
        FragmentLinkSRV[nNext] = element;
        nNext = element.nNext;
    }
}
