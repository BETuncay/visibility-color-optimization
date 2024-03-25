#include "shader_Common.hlsli"
#include "shader_ConstantBuffers.hlsli"
#include "shader_FragmentList_LowRes.hlsli"
#include "shader_QuadFormat.hlsli"

RWByteAddressBuffer StartOffsetSRV						: register( u1 );
RWStructuredBuffer< FragmentLink >  FragmentLinkSRV		: register( u2 );
RWByteAddressBuffer AlphaBufferUAV						: register( u3 );


QuadVS_Output VS( QuadVSinput Input )
{
    QuadVS_Output Output;
    Output.pos = Input.pos;
    return Output;
}

void PS( QuadPS_Input input )
{    
    // index to current pixel.
	uint nIndex = (uint)input.pos.y * ScreenWidth + (uint)input.pos.x;
	
    // get first fragment from the start offset buffer.
    uint nNext = StartOffsetSRV.Load(nIndex * 4); 
    
    // early exit if no fragments in the linked list.
    if( nNext == 0xFFFFFFFF ) {
		return;
    }


	float gall = 0;
	
	// First pass - iterate and sum up the squared importance
	[allow_uav_condition]
	for (int iii = 0; iii < TEMPORARY_BUFFER_MAX; iii++)
	{
		if ( nNext == 0xFFFFFFFF )
			break;
        FragmentLink element = FragmentLinkSRV[nNext];
        
		//float gi = clamp(( (element.fragmentData.uDepthImportance) & 0xFF ) / 255.0f, 0.001, 0.999);
		float gi = clamp(element.fragmentData.fImportance, 0.001, 0.999);
		gall = gall + gi*gi;

        nNext = element.nNext;
    }

	nNext = StartOffsetSRV.Load(nIndex * 4);
	float gf = 0;


    // second pass - compute the alpha values and write to file!
	[allow_uav_condition]
	for (int ii = 0; ii < TEMPORARY_BUFFER_MAX; ii++)
	{
		if ( nNext == 0xFFFFFFFF )
			break;
        FragmentLink element = FragmentLinkSRV[nNext];
        
		float gi = clamp(element.fragmentData.fImportance, 0.001, 0.999);
		float gb = gall - gf - gi*gi;

		float p = 1;
		float alpha = (p) / ( p + pow(saturate(1-gi), 2*Lambda) * ( R*gf + Q*gb ) );
		alpha = saturate(alpha);

		// which control point does this fragment belong to?
		int controlPoint = (int)floor(element.fragmentData.fAlphaWeight + 0.5f);	// all half left and right belongs to the control point.

		// if we have a valid control point, update the alpha value!
		if (controlPoint >= 0 && controlPoint < TotalNumberOfControlPoints)
		{
            uint temp;
			AlphaBufferUAV.InterlockedMin(controlPoint * 4u, asuint(alpha), temp);
		}

		gf = gf + gi*gi;

        nNext = element.nNext;
    }
}
