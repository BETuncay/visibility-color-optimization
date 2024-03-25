#include "shader_Common.hlsli"
#include "shader_ConstantBuffers.hlsli"

StructuredBuffer<float> Input : register(t0);
RWStructuredBuffer<float> Output : register(u0);


[numthreads(1, 1, 1)]
void CS(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    uint id = DTid.x;
    float target = Input[id];
    float current = Output[id];
    target += EPS;
    float next = lerp(current, target, 1.0f - pow(2.f, -dt / (DampingHalflife + EPS)));
    Output[id] = next;
}