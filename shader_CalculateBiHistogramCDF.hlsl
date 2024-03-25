#include "shader_Common.hlsli"


Texture2D<float> Input : register(t0);
RWStructuredBuffer<float> Output : register(u0);


[numthreads(1, 1, 1)]
void CS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    // calculate mean like the expected value of a discrete rv
    float scan_sum[HISTOGRAM_BINS];
    scan_sum[0] = Input.Load(int3(0, 0, 0));
    float median = 0;
    for (uint i = 1; i < HISTOGRAM_BINS; i++)
    {
        float histogramValue = Input.Load(int3(i, 0, 0));
        scan_sum[i] = scan_sum[i - 1] + histogramValue;
        median += histogramValue * i;
    }
    median /= scan_sum[HISTOGRAM_BINS - 1];
    uint medianIndex = (uint) median;
    
    // degenerate to default histogram cdf calculation
    if (medianIndex == 0 || medianIndex == HISTOGRAM_BINS - 1)
    {
        float normalize = rcp(scan_sum[HISTOGRAM_BINS - 1]);
        for (uint i = 0; i < HISTOGRAM_BINS; i++)
        {
            Output[i] = scan_sum[i] * normalize;
        }
    }
    else
    {
        // calculate CDF for first half
        // we already have the sum we only need to normalize
        float sum = 0;
        for (uint i = 0; i < medianIndex; i++)
        {
            sum += Input.Load(int3(i, 0, 0));
            Output[i] = sum;
        }
    
        float normalize = rcp(sum);
        for (i = 0; i < medianIndex; i++)
        {
            Output[i] = Output[i] * normalize;
        }
    
        // calculate CDF for second half
        // we need the sum to start with 0 -> subtract previous sum
        sum = 0;
        for (i = medianIndex; i < HISTOGRAM_BINS; i++)
        {
            sum += Input.Load(int3(i, 0, 0));
            Output[i] = sum;
        }
    
        normalize = rcp(sum);
        for (i = medianIndex; i < HISTOGRAM_BINS; i++)
        {
            Output[i] = Output[i] * normalize;
        }
    }
}