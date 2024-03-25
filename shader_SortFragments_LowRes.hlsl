#include "shader_Common.hlsli"
#include "shader_ConstantBuffers.hlsli"
#include "shader_FragmentList_LowRes.hlsli"
#include "shader_QuadFormat.hlsli"


RWByteAddressBuffer StartOffsetSRV					: register( u1 );
RWStructuredBuffer< FragmentLink >  FragmentLinkSRV	: register( u2 );


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

    FragmentData aData[ TEMPORARY_BUFFER_MAX ];           // temporary buffer
    uint nNumFragment = 0;                                // number of fragments in current pixel's linked list.
    uint nNext = StartOffsetSRV.Load(4 * nIndex);         // get first fragment from the start offset buffer.
    uint nFirst = nNext;
	float allAlpha = 1;

    // early exit if no fragments in the linked list.
    if( nNext == 0xFFFFFFFF ) {
		return;
    }

	
    // Read and store linked list data to the temporary buffer.    
	[allow_uav_condition]
	for (int ii = 0; ii < TEMPORARY_BUFFER_MAX; ii++)
	{
		if ( nNext == 0xFFFFFFFF )
			break;
        FragmentLink element = FragmentLinkSRV[nNext];
        aData[ nNumFragment ] = element.fragmentData;

        nNumFragment++;                   
        nNext = element.nNext;
    }
	
	// insertion sort
	[allow_uav_condition]
	for (uint jj = 1; jj < nNumFragment; jj++)
	{
		FragmentData valueToInsert = aData[jj];
		uint holePos;
		
		[allow_uav_condition]
		for (holePos=jj; holePos>0; holePos--)
		{
            if (valueToInsert.uDepth > aData[holePos - 1].uDepth)
                break;
			aData[holePos] = aData[holePos - 1];
		}
		aData[holePos] = valueToInsert;
	}
	
	// Store the sorted list. This is not efficient! Its better to update the Next-Links instead.
	nNext = nFirst;
	[allow_uav_condition]
	for( uint x = 0; x < nNumFragment; ++x )
	{
		FragmentLinkSRV[nNext].fragmentData = aData[ x ];		// front to back
		nNext = FragmentLinkSRV[nNext].nNext;
	}
}
