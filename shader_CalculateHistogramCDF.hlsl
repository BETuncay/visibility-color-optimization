#include "shader_Common.hlsli"


Texture2D<float> Input : register(t0);
RWStructuredBuffer<float> Output : register(u0);


#ifdef HISTOGRAM_CDF_SERIAL
[numthreads(1, 1, 1)]
void CS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    float sum = 0;
    for (uint i = 0; i < HISTOGRAM_BINS; i++)
    {
        sum += Input.Load(int3(i, 0, 0));
        Output[i] = sum;
    }
    
    float normalize = rcp(sum);
    for (i = 0; i < HISTOGRAM_BINS; i++)
    {
        Output[i] = Output[i] * normalize;
    }
}
#endif

#ifdef HISTOGRAM_CDF_PARALLEL
groupshared float Shared[HISTOGRAM_BINS][2];

// hills and steele double buffer
// https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-39-parallel-prefix-sum-scan-cuda
[numthreads(HISTOGRAM_BINS, 1, 1)]
void CS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint k = DTid.x;
   
    // load data into shared memory
    Shared[k][0] = Input.Load(int3(k, 0, 0));
    GroupMemoryBarrierWithGroupSync();

    uint depth = (uint) log2(HISTOGRAM_BINS);
    for (uint d = 0; d < depth; d++)
    {
        uint power = pow(2, d);
        if (k >= power)
        {
            Shared[k][1] = Shared[k][0] + Shared[k - power][0];
        }
        else
        {
            Shared[k][1] = Shared[k][0];
        }
        
        GroupMemoryBarrierWithGroupSync();
        
        Shared[k][0] = Shared[k][1];
        GroupMemoryBarrierWithGroupSync();
    }
    
    Output[k] = Shared[k][1];
    GroupMemoryBarrierWithGroupSync();

    float normalize = rcp(Output[HISTOGRAM_BINS - 1]);    
    Output[k] = Output[k] * normalize;
}
#endif