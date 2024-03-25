/*
Copyright(c) 2011 by Nathan Reed.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met :

1. Redistributions of source code must retain the above copyright notice,
this list of conditionsand the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditionsand the following disclaimer in the documentation
and /or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.IN NO
EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING, BUT NOT
	LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
	PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
	LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING NEGLIGENCE
		OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
Download Link: https://www.reedbeta.com/blog/gpu-profiling-101/#double-buffered-queries
*/
#include "gpuprofiler.hpp"
#include <d3d11.h>
#include <iostream>

float Time ()		// Retrieve time in seconds, using QueryPerformanceCounter or whatever
{
	LARGE_INTEGER timerCurrent, frequency;
	QueryPerformanceCounter(&timerCurrent);
	QueryPerformanceFrequency(&frequency);
	LONGLONG time = timerCurrent.QuadPart;;
	return  (float)time / (float)frequency.QuadPart;
}


// CGpuProfiler implementation

CGpuProfiler g_gpuProfiler;

CGpuProfiler::CGpuProfiler ()
:	m_iFrameQuery(0),
	m_iFrameCollect(-1),
	m_frameCountAvg(0),
	m_tBeginAvg(0.0f)
{
	memset(m_apQueryTsDisjoint, 0, sizeof(m_apQueryTsDisjoint));
	memset(m_apQueryTs, 0, sizeof(m_apQueryTs));
	memset(m_adT, 0, sizeof(m_adT));
	memset(m_adTAvg, 0, sizeof(m_adT));
	memset(m_adTTotalAvg, 0, sizeof(m_adT));
}

bool CGpuProfiler::Init (ID3D11Device* device)
{
	// Create all the queries we'll need

	D3D11_QUERY_DESC queryDesc = { D3D11_QUERY_TIMESTAMP_DISJOINT, 0 };

	if (FAILED(device->CreateQuery(&queryDesc, &m_apQueryTsDisjoint[0])))
	{
		std::cout << "Could not create timestamp disjoint query for frame 0!" << '\n';
		return false;
	}

	if (FAILED(device->CreateQuery(&queryDesc, &m_apQueryTsDisjoint[1])))
	{
		std::cout << "Could not create timestamp disjoint query for frame 1!" << '\n';
		return false;
	}

	queryDesc.Query = D3D11_QUERY_TIMESTAMP;

	for (GTS gts = GTS_BeginFrame; gts < GTS_Max; gts = GTS(gts + 1))
	{
		if (FAILED(device->CreateQuery(&queryDesc, &m_apQueryTs[gts][0])))
		{
			std::cout << "Could not create start-frame timestamp query for GTS %d, frame 0!" << " " << gts << '\n';
			return false;
		}

		if (FAILED(device->CreateQuery(&queryDesc, &m_apQueryTs[gts][1])))
		{
			std::cout << "Could not create start-frame timestamp query for GTS %d, frame 1!" << " " << gts << '\n';
			return false;
		}
	}

	return true;
}

void CGpuProfiler::Shutdown ()
{
	if (m_apQueryTsDisjoint[0])
		m_apQueryTsDisjoint[0]->Release();

	if (m_apQueryTsDisjoint[1])
		m_apQueryTsDisjoint[1]->Release();

	for (GTS gts = GTS_BeginFrame; gts < GTS_Max; gts = GTS(gts + 1))
	{
		if (m_apQueryTs[gts][0])
			m_apQueryTs[gts][0]->Release();

		if (m_apQueryTs[gts][1])
			m_apQueryTs[gts][1]->Release();
	}
}

void CGpuProfiler::BeginFrame (ID3D11DeviceContext* immediateContext)
{
	immediateContext->Begin(m_apQueryTsDisjoint[m_iFrameQuery]);
	Timestamp(GTS_BeginFrame, immediateContext);
}

void CGpuProfiler::Timestamp (GTS gts, ID3D11DeviceContext* immediateContext)
{
	immediateContext->End(m_apQueryTs[gts][m_iFrameQuery]);
}

void CGpuProfiler::EndFrame (ID3D11DeviceContext* immediateContext)
{
	Timestamp(GTS_EndFrame, immediateContext);
	immediateContext->End(m_apQueryTsDisjoint[m_iFrameQuery]);

	++m_iFrameQuery &= 1;
}

void CGpuProfiler::WaitForDataAndUpdate (ID3D11DeviceContext* immediateContext)
{
	if (m_iFrameCollect < 0)
	{
		// Haven't run enough frames yet to have data
		m_iFrameCollect = 0;
		return;
	}

	// Wait for data
	while (immediateContext->GetData(m_apQueryTsDisjoint[m_iFrameCollect], NULL, 0, 0) == S_FALSE)
	{
		Sleep(1);
	}

	int iFrame = m_iFrameCollect;
	++m_iFrameCollect &= 1;

	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timestampDisjoint;
	if (immediateContext->GetData(m_apQueryTsDisjoint[iFrame], &timestampDisjoint, sizeof(timestampDisjoint), 0) != S_OK)
	{
		std::cout << "Couldn't retrieve timestamp disjoint query data";
		return;
	}

	if (timestampDisjoint.Disjoint)
	{
		// Throw out this frame's data
		std::cout << "Timestamps disjoint";
		return;
	}

	UINT64 timestampPrev;
	if (immediateContext->GetData(m_apQueryTs[GTS_BeginFrame][iFrame], &timestampPrev, sizeof(UINT64), 0) != S_OK)
	{
		std::cout << "Couldn't retrieve timestamp query data for GTS %d" << " " << GTS_BeginFrame << '\n';
		return;
	}

	for (GTS gts = GTS(GTS_BeginFrame + 1); gts < GTS_Max; gts = GTS(gts + 1))
	{
		UINT64 timestamp;
		if (immediateContext->GetData(m_apQueryTs[gts][iFrame], &timestamp, sizeof(UINT64), 0) != S_OK)
		{
			std::cout <<  "Couldn't retrieve timestamp query data for GTS %d" << " "<< gts << '\n';
			return;
		}

		m_adT[gts] = float(timestamp - timestampPrev) / float(timestampDisjoint.Frequency);
		timestampPrev = timestamp;

		m_adTTotalAvg[gts] += m_adT[gts];
	}

	++m_frameCountAvg;
	if (Time() > m_tBeginAvg + 0.5f)
	{
		for (GTS gts = GTS_BeginFrame; gts < GTS_Max; gts = GTS(gts + 1))
		{
			m_adTAvg[gts] = m_adTTotalAvg[gts] / m_frameCountAvg;
			m_adTTotalAvg[gts] = 0.0f;
		}

		m_frameCountAvg = 0;
		m_tBeginAvg = Time();
	}
}
