#include "shader_Common.hlsli"
#include "shader_ConstantBuffers.hlsli"

//-------------------------------------------------------------------------
// Parameter
//-------------------------------------------------------------------------


struct FragmentData
{
    uint nColor; // Pixel color
    uint nDepth; // Depth 
    uint nCoverage; // Coverage
};

struct FragmentLink
{
    FragmentData fragmentData; // Fragment data
    uint nNext; // Link to next fragment
};

struct HistogramData
{
    float scalarColor;
    float transmittance;
};

RWStructuredBuffer<FragmentLink> FLBuffer : register(u1);
RWByteAddressBuffer StartOffsetBuffer : register(u2);
RWByteAddressBuffer HistogramBuffer : register(u3);

Texture2D<float> DepthBuffer : register(t0);


//-------------------------------------------------------------------------
// Layouts
//-------------------------------------------------------------------------

struct VS_INPUT10
{
    float3 PositionA : POSITIONA;
    float3 PositionB : POSITIONB;
    int IDA : IDA;
    int IDB : IDB;
    float AlphaA : ALPHAA;
    float AlphaB : ALPHAB;
    float ScalarColorA : SCALARCOLORA;
    float ScalarColorB : SCALARCOLORB;
};

struct GS_INPUT10
{
    float4 PositionA : POSITIONA;
    float4 PositionB : POSITIONB;
    int IDA : IDA;
    int IDB : IDB;
    float AlphaA : ALPHAA;
    float AlphaB : ALPHAB;
    float ScalarColorA : SCALARCOLORA;
    float ScalarColorB : SCALARCOLORB;
};

struct GS_OUTPUT10
{
    float4 Position : SV_POSITION;
    float3 VRCPosition : TEXCOORD0;
    float3 VRCDirection : TEXCOORD1;
    float Alpha : ALPHA;
    float2 TexCoord : TEXCOORD2;
    float ScalarColor : SCALARCOLOR;
};

struct PS_INPUT10
{
    float4 Position : SV_POSITION;
    float3 VRCPosition : TEXCOORD0;
    float3 VRCDirection : TEXCOORD1;
    float Alpha : ALPHA;
    float2 TexCoord : TEXCOORD2;
    float ScalarColor : SCALARCOLOR;
};

//--------------------------------------------------------------------------------------
// Vertex Shader
//--------------------------------------------------------------------------------------
GS_INPUT10 VS(VS_INPUT10 input)
{
    GS_INPUT10 output;
    output.PositionA = mul(float4(input.PositionA, 1), View);
    output.PositionB = mul(float4(input.PositionB, 1), View);
    output.IDA = input.IDA;
    output.IDB = input.IDB;
    output.AlphaA = input.AlphaA;
    output.AlphaB = input.AlphaB;
    output.ScalarColorA = input.ScalarColorA;
    output.ScalarColorB = input.ScalarColorB;
    return output;
}

//--------------------------------------------------------------------------------------
// Geometry Shader
//--------------------------------------------------------------------------------------
[maxvertexcount(4)]
void GS(line GS_INPUT10 input[2], inout TriangleStream<GS_OUTPUT10> output)
{
    if (input[0].IDA == input[0].IDB && input[0].IDB == input[1].IDA && input[1].IDA == input[1].IDB)
    {
        GS_OUTPUT10 vertex[4];
	
        float4 p1 = input[0].PositionB.xyzw / input[0].PositionB.w;
        float4 p0 = input[1].PositionA.xyzw / input[1].PositionA.w;

        float4 a0 = input[0].PositionA.xyzw / input[0].PositionA.w;
        float4 a1 = input[1].PositionB.xyzw / input[1].PositionB.w;

        float t0 = 0;
        float t1 = 0;

        float3 dir3_0 = normalize(p1.xyz - a0.xyz);
        float3 dir3_1 = normalize(a1.xyz - p0.xyz);

        float2 dir0 = normalize(p1.xy - a0.xy) * StripWidth;
        float2 dir1 = normalize(a1.xy - p0.xy) * StripWidth;
        float4 off0 = float4(-dir0.y, dir0.x, 0, 0);
        float4 off1 = float4(-dir1.y, dir1.x, 0, 0);

        float alpha0 = 1 - input[1].AlphaA;
        float alpha1 = 1 - input[0].AlphaB;

		// reject almost invisible segments
        if (alpha0 > 0.99 && alpha1 > 0.99)
            return;

        vertex[0].Position = p0 + off0;
        vertex[0].TexCoord = float2(0, t0);
        vertex[0].VRCDirection = dir3_0;
        vertex[0].Alpha = alpha0;
        vertex[0].ScalarColor = input[1].ScalarColorA;

        vertex[1].Position = p0 - off0;
        vertex[1].TexCoord = float2(1, t0);
        vertex[1].VRCDirection = dir3_0;
        vertex[1].Alpha = alpha0;
        vertex[1].ScalarColor = input[1].ScalarColorA;

        vertex[2].Position = p1 + off1;
        vertex[2].TexCoord = float2(0, t1);
        vertex[2].VRCDirection = dir3_1;
        vertex[2].Alpha = alpha1;
        vertex[2].ScalarColor = input[0].ScalarColorB;

        vertex[3].Position = p1 - off1;
        vertex[3].TexCoord = float2(1, t1);
        vertex[3].VRCDirection = dir3_1;
        vertex[3].Alpha = alpha1;
        vertex[3].ScalarColor = input[0].ScalarColorB;

        for (int i = 0; i < 4; i++)
        {
            vertex[i].VRCPosition = vertex[i].Position.xyz;
            vertex[i].Position = mul(vertex[i].Position, Projection);
            output.Append(vertex[i]);
        }
        output.RestartStrip();
    }
}

//--------------------------------------------------------------------------------------
// Pixel Shader
//--------------------------------------------------------------------------------------

void computeColor(float2 texCoord, float3 vrcPositionNormalized, float3 vrcDirection, out float4 color)
{
    float halfDistCenter = abs(texCoord.x - 0.5);
    if (halfDistCenter * 2 > HaloPortion)
    {
        color = HaloColor;
    }
    else
    {
        float4 res = LineColor;
        float3 LT = vrcPositionNormalized;
        float3 VT = vrcPositionNormalized;
        float DL = dot(vrcDirection, LT);
        float DV = dot(vrcDirection, VT);
        float fDiff = sqrt(1.0f - DL * DL);
        float fSpec = pow(abs(sqrt(1.0f - DL * DL) * sqrt(1.0f - DV * DV) - DL * DV), 64);
        color = float4(saturate((0.2 + 0.6 * fDiff) * res.xyz + 0.4 * fSpec.xxx), res.a);
    }
}


void HStore(uint address, HistogramData data)
{
    HistogramBuffer.Store(address * 8, asuint(0.9)); //asuint(data.scalarColor));
    HistogramBuffer.Store(address * 8 + 4, asuint(data.transmittance));
}

void PS(PS_INPUT10 input)
{
    float4 color = float4(0, 0, 0, 0);
    float depth = 0;
    computeColor(input.TexCoord, normalize(input.VRCPosition), input.VRCDirection, color);

    float halfDistCenter = abs(input.TexCoord.x - 0.5);
    if (halfDistCenter * 2 > HaloPortion)
    {
        float offset = (halfDistCenter) * 2 * StripWidth;
        float4 offsetPosNPC = mul(float4(input.VRCPosition, 1) + float4(0, 0, offset, 0), Projection);
        depth = offsetPosNPC.z / offsetPosNPC.w;
    }
    else
    {
        float4 offsetPosNPC = mul(float4(input.VRCPosition, 1), Projection);
        depth = offsetPosNPC.z / offsetPosNPC.w;
    }

    float transparency = input.Alpha;
    uint x = input.Position.x;
    uint y = input.Position.y;

    //float refDepth = DepthBuffer.Load(uint3(input.Position.xy, 0));
    //if (true || depth < refDepth)
    {
		// Create fragment data.
        uint4 nColor = saturate(float4(color.rgb, transparency)) * 255;
        FragmentLink element;
        element.fragmentData.nColor = (nColor.x) | (nColor.y << 8) | (nColor.z << 16) | (nColor.a << 24);
        element.fragmentData.nCoverage = 0;
        element.fragmentData.nDepth = asuint(depth);

		// Increment and get current pixel count.
        uint nPixelCount = FLBuffer.IncrementCounter();

		// Read and update Start Offset Buffer.
        uint nIndex = y * ScreenWidth + x;
        uint nStartOffsetAddress = 4 * nIndex;
        uint nOldStartOffset;
    
        StartOffsetBuffer.InterlockedExchange(
			nStartOffsetAddress, nPixelCount, nOldStartOffset);

        
		// Store fragment link.
        element.nNext = nOldStartOffset;
        FLBuffer[nPixelCount] = element;
        
		// Histogram store data
        uint nHistogramAddress = 8 * nPixelCount;
        HistogramData hData;
        hData.scalarColor = input.ScalarColor;
        hData.transmittance = transparency;
        //HStore(nHistogramAddress, hData);

        //HistogramBuffer.Store(nPixelCount * 4, 1);
        uint trash;
        HistogramBuffer.InterlockedExchange(nPixelCount * 4, 1, trash);
        HistogramBuffer.Store(nHistogramAddress + 4, 2);
    }
}
