#include "shader_Common.hlsli"
#include "shader_ConstantBuffers.hlsli"

Texture2D<float> Input : register(t0);
RWStructuredBuffer<float> Output : register(u0);


[numthreads(1, 1, 1)]
void CS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    // calculate prefix sum of input texture
    float scan_sum[HISTOGRAM_BINS];
    scan_sum[0] = Input.Load(int3(0, 0, 0));
    for (uint i = 1; i < HISTOGRAM_BINS; i++)
    {
        scan_sum[i] = scan_sum[i - 1] + Input.Load(int3(i, 0, 0));
    }
    
    // calculate segmented CDF
    float HistogramSegmentsFloat[HISTOGRAM_SEGMENTS_MAX] = (float[HISTOGRAM_SEGMENTS_MAX]) HistogramSegments; // cast array to useable format
    for (uint ii = 1; ii < HISTOGRAM_SEGMENTS_MAX; ii++)                                                      // start at 1 -> first element always 0
    {
        if (HistogramSegmentsFloat[ii] == -1.f)
        {
            break;
        }
        
        uint index = (uint) floor((HistogramSegmentsFloat[ii] * (HISTOGRAM_BINS)));
        uint previousIndex = (uint) floor((HistogramSegmentsFloat[ii - 1] * (HISTOGRAM_BINS)));

        for (uint iii = previousIndex; iii < index; iii++)
        {
            Output[iii] = scan_sum[iii];
        }
        
        float max_value = Output[index - 1];
        float normalize = rcp(max_value);
        for (uint iii = previousIndex; iii < index; iii++)
        {
            Output[iii] *= normalize;
        }
        
        // need to adjust scan_sum for next cdf value
        for (uint iiii = index; iiii < HISTOGRAM_BINS; iiii++)
        {
            scan_sum[iiii] -= max_value;
        }
    }    
}