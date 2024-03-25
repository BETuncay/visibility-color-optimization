#include "renderer.hpp"
#include "gpuprofiler.hpp"


Renderer::Renderer(float q, float r, float lambda, float stripWidth, int smoothingIterations, HistogramMode histogramMode, std::vector<float> histogramSegments, ID3D11Device* device, const DXGI_SURFACE_DESC* BackBufferSurfaceDesc) :
	_VbViewportQuad(NULL),
	_InputLayout_Line_HQ(NULL),
	_InputLayout_Line_LowRes(NULL),
	_InputLayout_ViewportQuad(NULL),
	_ResolutionDownScale(1),
	_SmoothingIterations(smoothingIterations),
	mDampingHalflife(0.8),
	mHistogramMode(histogramMode),
	mLineShader_HQ(),
	mLineShader_LowRes(),
	mMinGather_LowRes(),
	mRenderFragments(),
	mSortFragments(),
	mSortFragments_LowRes(),
	mCalculateHistogram(),
	mDebugQuad(),
	mDebugGeometry(),
	mFadeAlpha(),
	mSmoothAlpha(),
	mCalculateHistogramCDF(),
	mCalculateBiHistogramCDF(),
	mCalculateSegmentedHistogramCDF(),
	mReplaceScalarColor()
{
	_CbRenderer.Data.Q = q;
	_CbRenderer.Data.R = r;
	_CbRenderer.Data.Lambda = lambda;
	_CbRenderer.Data.StripWidth = stripWidth;

	_CbRenderer.Data.HistogramMode = mHistogramMode;
	if (histogramSegments.size() > HISTOGRAM_SEGMENTS_MAX)
	{
		return;
	}

	for (int i = 0; i < histogramSegments.size(); i++)
	{
		_CbRenderer.Data.HistogramSegments[i] = histogramSegments[i];
	}
	for (int i = histogramSegments.size(); i < HISTOGRAM_SEGMENTS_MAX; i++)
	{
		_CbRenderer.Data.HistogramSegments[i] = -1.f;
	}
		
	D3DCreateDevice(device);
	D3DCreateSwapChain(device, BackBufferSurfaceDesc);
}

Renderer::~Renderer()
{
	D3DReleaseDevice();
	D3DReleaseSwapChain();
}

// create shaders, input layout, quad buffer, constant buffers
bool Renderer::D3DCreateDevice(ID3D11Device* Device)
{
	if (!mLineShader_HQ.D3DCreate(			Device, "shader_CreateLists_HQ",			vislab::VS_FLAG | vislab::GS_FLAG | vislab::PS_FLAG)) return false;
	if (!mLineShader_LowRes.D3DCreate(		Device, "shader_CreateLists_LowRes",		vislab::VS_FLAG | vislab::GS_FLAG | vislab::PS_FLAG)) return false;
	if (!mDebugGeometry.D3DCreate(			Device, "shader_DebugGeometry",				vislab::VS_FLAG | vislab::GS_FLAG | vislab::PS_FLAG)) return false;

	if (!mMinGather_LowRes.D3DCreate(		Device, "shader_MinGather_LowRes",			vislab::VS_FLAG | vislab::PS_FLAG)) return false;
	if (!mRenderFragments.D3DCreate(		Device, "shader_RenderFragments",			vislab::VS_FLAG | vislab::PS_FLAG)) return false;
	if (!mSortFragments.D3DCreate(			Device, "shader_SortFragments",				vislab::VS_FLAG | vislab::PS_FLAG)) return false;
	if (!mSortFragments_LowRes.D3DCreate(	Device, "shader_SortFragments_LowRes",		vislab::VS_FLAG | vislab::PS_FLAG)) return false;
	if (!mCalculateHistogram.D3DCreate(		Device, "shader_CalculateHistogram",		vislab::VS_FLAG | vislab::PS_FLAG)) return false;
	if (!mDebugQuad.D3DCreate(				Device, "shader_DebugQuad",					vislab::VS_FLAG | vislab::PS_FLAG)) return false;
	if (!mReplaceScalarColor.D3DCreate(		Device, "shader_ReplaceScalarColor",		vislab::VS_FLAG | vislab::PS_FLAG)) return false;

	if (!mFadeAlpha.D3DCreate(						Device, "shader_FadeToAlphaPerVertex",					vislab::CS_FLAG)) return false;
	if (!mSmoothAlpha.D3DCreate(					Device, "shader_SmoothAlpha",							vislab::CS_FLAG)) return false;
	if (!mCalculateHistogramCDF.D3DCreate(			Device, "shader_CalculateHistogramCDF",					vislab::CS_FLAG)) return false;
	if (!mCalculateBiHistogramCDF.D3DCreate(		Device, "shader_CalculateBiHistogramCDF",				vislab::CS_FLAG)) return false;
	if (!mCalculateSegmentedHistogramCDF.D3DCreate(	Device, "shader_CalculateSegmentedHistogramCDF",		vislab::CS_FLAG)) return false;
	if (!mExponentialDamping.D3DCreate(				Device, "shader_ExponentialDamping",					vislab::CS_FLAG)) return false;

	// Create input layout
	{
		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITIONA",		0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,						D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "POSITIONB",		0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(XMFLOAT3) * 2,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "IDA",			0, DXGI_FORMAT_R32_SINT,		1, 0,						D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "IDB",			0, DXGI_FORMAT_R32_SINT,		1, sizeof(int) * 2,			D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "ALPHAA",			0, DXGI_FORMAT_R32_FLOAT,		2, 0,						D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "ALPHAB",			0, DXGI_FORMAT_R32_FLOAT,		2, sizeof(float) * 2,		D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "SCALARCOLORA",	0, DXGI_FORMAT_R32_FLOAT,		3, 0,						D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "SCALARCOLORB",	0, DXGI_FORMAT_R32_FLOAT,		3, sizeof(float) * 2,		D3D11_INPUT_PER_VERTEX_DATA, 0 },

		};
		HRESULT hr = Device->CreateInputLayout(layout, 8, mLineShader_HQ.GetBlob(), mLineShader_HQ.GetSize(), &_InputLayout_Line_HQ);
		if (FAILED(hr))	return false;
	}

	{
		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITIONA",		0, DXGI_FORMAT_R32G32B32_FLOAT,	0, 0,						D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "POSITIONB",		0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(XMFLOAT3) * 2,	D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "IDA",			0, DXGI_FORMAT_R32_SINT,		1, 0,						D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "IDB",			0, DXGI_FORMAT_R32_SINT,		1, sizeof(int) * 2,			D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "IMPORTANCEA",	0, DXGI_FORMAT_R32_FLOAT,		2, 0,						D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "IMPORTANCEB",	0, DXGI_FORMAT_R32_FLOAT,		2, sizeof(float) * 2,		D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "ALPHAWEIGHTA",	0, DXGI_FORMAT_R32_FLOAT,		3, 0,						D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "ALPHAWEIGHTB",	0, DXGI_FORMAT_R32_FLOAT,		3, sizeof(float) * 2,		D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		HRESULT hr = Device->CreateInputLayout(layout, 8, mLineShader_LowRes.GetBlob(), mLineShader_LowRes.GetSize(), &_InputLayout_Line_LowRes);
		if (FAILED(hr))	return false;
	}

	{
		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		HRESULT hr = Device->CreateInputLayout(layout, 1, mSortFragments.GetBlob(), mSortFragments.GetSize(), &_InputLayout_ViewportQuad);
		if (FAILED(hr))	return false;
	}

	{
		D3D11_BUFFER_DESC bufDesc;
		ZeroMemory(&bufDesc, sizeof(D3D11_BUFFER_DESC));
		bufDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bufDesc.ByteWidth = 6 * sizeof(XMFLOAT3);
		bufDesc.CPUAccessFlags = 0;
		bufDesc.MiscFlags = 0;
		bufDesc.StructureByteStride = sizeof(XMFLOAT3);
		bufDesc.Usage = D3D11_USAGE_DEFAULT;

		XMFLOAT3 positions[] = {
			XMFLOAT3(-1, -1, 0),
			XMFLOAT3(-1,  1, 0),
			XMFLOAT3(1, -1, 0),
			XMFLOAT3(-1,  1, 0),
			XMFLOAT3(1,  1, 0),
			XMFLOAT3(1, -1, 0),
		};
		D3D11_SUBRESOURCE_DATA initData;
		ZeroMemory(&initData, sizeof(D3D11_SUBRESOURCE_DATA));
		initData.pSysMem = positions;

		if (FAILED(Device->CreateBuffer(&bufDesc, &initData, &_VbViewportQuad))) return false;
	}

	if (!_CbFadeToAlpha.Create(Device)) return false;
	if (!_CbRenderer.Create(Device)) return false;

	D3D11_SAMPLER_DESC sampDesc;
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampDesc.MipLODBias = 0;
	sampDesc.MaxAnisotropy = 0;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
	if (FAILED(Device->CreateSamplerState(&sampDesc, &mSamplerState)))
		return false;

	mLineShader_HQ.FreeBlob();
	mLineShader_LowRes.FreeBlob();
	mMinGather_LowRes.FreeBlob();
	mRenderFragments.FreeBlob();
	mSortFragments.FreeBlob();
	mSortFragments_LowRes.FreeBlob();
	mCalculateHistogram.FreeBlob();
	mDebugQuad.FreeBlob();
	mDebugGeometry.FreeBlob();
	mCalculateHistogramCDF.FreeBlob();
	mCalculateBiHistogramCDF.FreeBlob();
	mCalculateSegmentedHistogramCDF.FreeBlob();
	mReplaceScalarColor.FreeBlob();
	return true;
}

// create buffers and textures
bool Renderer::D3DCreateSwapChain(ID3D11Device* Device, const DXGI_SURFACE_DESC* BackBufferSurfaceDesc)
{
	unsigned int NUM_ELEMENTS = BackBufferSurfaceDesc->Width * BackBufferSurfaceDesc->Height;
	unsigned int NUM_ELEMENTS_LOW_RES = NUM_ELEMENTS / (_ResolutionDownScale * _ResolutionDownScale);
	unsigned int NUM_ELEMENTS_OVERDRAW = NUM_ELEMENTS * EXPECTED_OVERDRAW_IN_LINKED_LISTS;
	unsigned int NUM_ELEMENTS_OVERDRAW_LOW_RES = NUM_ELEMENTS_LOW_RES * EXPECTED_OVERDRAW_IN_LINKED_LISTS;

	// -------------------------------------- create start offset buffers
	{
		// --- create start offset buffer
		vislab::BufferDesc desc;
		desc.Num_Elements = NUM_ELEMENTS;
		desc.Size_Element = sizeof(unsigned int);
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		desc.CPUAccessFlag = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		desc.Format = DXGI_FORMAT_R32_TYPELESS;
		desc.Uav_Flags = D3D11_BUFFER_UAV_FLAG_RAW;
		mStartOffsetBuffer = vislab::BufferD3D11(desc);
		if (!mStartOffsetBuffer.D3DCreate(Device))
			return false;

		// --- create start offset buffer low resolution
		desc.Num_Elements = NUM_ELEMENTS_LOW_RES;
		mStartOffsetBufferLowRes = vislab::BufferD3D11(desc);
		if (!mStartOffsetBufferLowRes.D3DCreate(Device))
			return false;
	}

	// --------------------------------- create fragment link list buffers
	{
		// --- create fragment link list 
		vislab::BufferDesc desc;
		desc.Num_Elements = NUM_ELEMENTS_OVERDRAW;
		desc.Size_Element = FragmentLink::GetSizeInBytes();
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlag = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.Uav_Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
		mFragmentLinkBuffer = vislab::BufferD3D11(desc);
		if (!mFragmentLinkBuffer.D3DCreate(Device))
			return false;

		// --- create fragment link list low resolution
		desc.Num_Elements = NUM_ELEMENTS_OVERDRAW_LOW_RES;
		desc.Size_Element = FragmentLinkLowRes::GetSizeInBytes();
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		mFragmentLinkBufferLowRes = vislab::BufferD3D11(desc);
		if (!mFragmentLinkBufferLowRes.D3DCreate(Device))
			return false;
	}

	// create histogram render target and texture
	{
		vislab::RenderTarget2dD3D11::TargetDesc desc;
		desc.Resolution = Eigen::Vector2i(HISTOGRAM_BINS, 1);
		desc.Format = DXGI_FORMAT_R32_FLOAT;
		desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		desc.AccessFlag = D3D11_CPU_ACCESS_READ;
		mHistogramTarget = vislab::RenderTarget2dD3D11(desc);
		if (!mHistogramTarget.D3DCreate(Device))
			return false;

		// create normalized version for imgui
		mHistogramTargetNormalized = vislab::RenderTarget2dD3D11(desc);
		if (!mHistogramTargetNormalized.D3DCreate(Device))
			return false;
	}

	// create fragment list counter
	{
		vislab::BufferDesc desc;
		desc.Num_Elements = 4;
		desc.Size_Element = sizeof(unsigned int);
		desc.BindFlags = 0;
		desc.CPUAccessFlag = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
		mFragmentCounterBuffer = vislab::BufferD3D11(desc);
		if (!mFragmentCounterBuffer.D3DCreate(Device))
			return false;
	}

	// create histogram cdf buffer
	{
		vislab::BufferDesc desc;
		desc.Num_Elements = HISTOGRAM_BINS;
		desc.Size_Element = sizeof(float);
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlag = D3D11_CPU_ACCESS_READ;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		mHistogramCDF = vislab::BufferD3D11(desc);
		if (!mHistogramCDF.D3DCreate(Device))
			return false;

		// create buffer that smoothly follow the changing histogram CDF values
		mHistogramCDFExponentialDamping = vislab::BufferD3D11(desc);
		if (!mHistogramCDFExponentialDamping.D3DCreate(Device))
			return false;

		// create buffer that contains the CDF value of the previous frame
		mHistogramCDFPrev = vislab::BufferD3D11(desc);
		if (!mHistogramCDFPrev.D3DCreate(Device))
			return false;

		// create normalized version for imgui
		mHistogramCDFNormalized = vislab::BufferD3D11(desc);
		if (!mHistogramCDFNormalized.D3DCreate(Device))
			return false;
	}
	return true;
}

// release shaders and input layout
void Renderer::D3DReleaseDevice()
{
	if (_InputLayout_Line_HQ)		_InputLayout_Line_HQ->Release();		_InputLayout_Line_HQ = NULL;
	if (_InputLayout_Line_LowRes)	_InputLayout_Line_LowRes->Release();	_InputLayout_Line_LowRes = NULL;
	if (_InputLayout_ViewportQuad)	_InputLayout_ViewportQuad->Release();	_InputLayout_ViewportQuad = NULL;
	if (_VbViewportQuad) 			_VbViewportQuad->Release();				_VbViewportQuad = NULL;

	mLineShader_HQ.D3DRelease();
	mLineShader_LowRes.D3DRelease();
	mMinGather_LowRes.D3DRelease();
	mRenderFragments.D3DRelease();
	mSortFragments.D3DRelease();
	mSortFragments_LowRes.D3DRelease();
	mCalculateHistogram.D3DRelease();
	mDebugQuad.D3DRelease();
	mDebugGeometry.D3DRelease();
	mFadeAlpha.D3DRelease();
	mSmoothAlpha.D3DRelease();
	mCalculateHistogramCDF.D3DRelease();
	mCalculateBiHistogramCDF.D3DRelease();
	mCalculateSegmentedHistogramCDF.D3DRelease();
	mReplaceScalarColor.D3DRelease();
	mExponentialDamping.D3DRelease();
	_CbFadeToAlpha.Release();
	_CbRenderer.Release();
}

// release buffers and textures
void Renderer::D3DReleaseSwapChain()
{
	mStartOffsetBuffer.D3DRelease();
	mStartOffsetBufferLowRes.D3DRelease();
	mFragmentLinkBuffer.D3DRelease();
	mFragmentLinkBufferLowRes.D3DRelease();
	mFragmentCounterBuffer.D3DRelease();
	mHistogramCDF.D3DRelease();
	mHistogramCDFExponentialDamping.D3DRelease();
	mHistogramCDFPrev.D3DRelease();
	mHistogramCDFNormalized.D3DRelease();
	mHistogramTarget.D3DRelease();
	mHistogramTargetNormalized.D3DRelease();
}

// move somewhere else maybe
void Renderer::InitBufferData(ID3D11DeviceContext* ImmediateContext)
{
	D3D11_DRAW_INSTANCED_INDIRECT_ARGS args = {};
	args.VertexCountPerInstance = 0;
	args.InstanceCount = 1;
	args.StartVertexLocation = 0;
	args.StartInstanceLocation = 0;
	ImmediateContext->UpdateSubresource(mFragmentCounterBuffer.GetBuf(), 0, nullptr, &args, 0, 0);
}

void Renderer::BindQuad(ID3D11DeviceContext* ImmediateContext)
{
	ImmediateContext->IASetInputLayout(_InputLayout_ViewportQuad);
	ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D11Buffer* vbs[] = { _VbViewportQuad, NULL, NULL, NULL };
	UINT strides[] = { sizeof(XMFLOAT3), 0, 0, 0 };
	UINT offsets[] = { 0, 0, 0, 0 };
	ImmediateContext->IASetVertexBuffers(0, 4, vbs, strides, offsets);
}

void Renderer::Draw(ID3D11DeviceContext* ImmediateContext, D3D* D3D, Lines* Geometry, Camera* Camera, Colormap* g_Colormap, float dt)
{
	static int ping = 0;
	static unsigned int clearUav[4] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };

	_CbFadeToAlpha.UpdateBuffer(ImmediateContext);

	_CbRenderer.Data.TotalNumberOfControlPoints = Geometry->GetTotalNumberOfControlPoints();
	_CbRenderer.Data.ScreenWidth = (int)D3D->GetBackBufferSurfaceDesc().Width;
	_CbRenderer.Data.ScreenHeight = (int)D3D->GetBackBufferSurfaceDesc().Height;
	_CbRenderer.Data.dt = dt;
	_CbRenderer.UpdateBuffer(ImmediateContext);
	Camera->GetParams().UpdateBuffer(ImmediateContext);

	ID3D11Buffer* cbs[] = { Camera->GetParams().GetBuffer(), _CbRenderer.GetBuffer() };
	ImmediateContext->VSSetConstantBuffers(0, 2, cbs);
	ImmediateContext->GSSetConstantBuffers(0, 2, cbs);
	ImmediateContext->PSSetConstantBuffers(0, 2, cbs);

	ID3D11RenderTargetView* rtvs[] = { D3D->GetRtvBackbuffer() };
	float blendFactor[4] = { 1,1,1,1 };

	const D3D11_VIEWPORT& fullViewport = D3D->GetFullViewport();

	// switch to smaller viewport resolution
	D3D11_VIEWPORT smallViewport = fullViewport;
	smallViewport.Width /= _ResolutionDownScale;
	smallViewport.Height /= _ResolutionDownScale;
	ImmediateContext->RSSetViewports(1, &smallViewport);

	D3D11_VIEWPORT binViewport = fullViewport;
	binViewport.Width = HISTOGRAM_BINS;
	binViewport.Height = 1;

	ImmediateContext->CopyResource(mHistogramCDFPrev.GetBuf(), mHistogramCDF.GetBuf()); // remember CDF values of the previous frame

#pragma region Create fragment linked lists - low res
	{
		// Clear the start offset buffer by magic value.
		ImmediateContext->ClearUnorderedAccessViewUint(mStartOffsetBufferLowRes.GetUav(), clearUav);

		// Bind states
		ImmediateContext->IASetInputLayout(_InputLayout_Line_LowRes);
		ID3D11RenderTargetView* rtvsNo[] = { NULL };
		ImmediateContext->OMSetRenderTargets(0, rtvsNo, NULL);
		ImmediateContext->OMSetBlendState(D3D->GetBsDefault(), blendFactor, 0xffffffff); // blendFactor not used
		ImmediateContext->OMSetDepthStencilState(D3D->GetDsTestWriteOff(), 0);
		ImmediateContext->RSSetState(D3D->GetRsCullNone());

		mLineShader_LowRes.SetShader(ImmediateContext);

		ID3D11ShaderResourceView* srvs[] = { D3D->GetSrvDepthbuffer() };
		ImmediateContext->PSSetShaderResources(0, 1, srvs);

		ID3D11UnorderedAccessView* uavs[] = { mFragmentLinkBufferLowRes.GetUav(), mStartOffsetBufferLowRes.GetUav() };
		UINT initialCount[] = { 0,0,0,0 };
		// render target is only bound so that the rasterizer knows, how many samples we want. We won't render into it, yet.
		ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, rtvs, NULL, 1, 2, uavs, initialCount);

		// Render
		Geometry->DrawLowRes(ImmediateContext);

		ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL, NULL };
		ImmediateContext->GSSetShaderResources(0, 1, noSrvs);
		ImmediateContext->PSSetShaderResources(0, 3, noSrvs);
	}
#pragma endregion
	g_gpuProfiler.Timestamp(GTS_CreateListLow, ImmediateContext);
#pragma region Sort the fragments - low res
	{

		mSortFragments_LowRes.SetShader(ImmediateContext);

		ID3D11UnorderedAccessView* uavs[] = { mStartOffsetBufferLowRes.GetUav(), mFragmentLinkBufferLowRes.GetUav() };
		UINT initialCount[] = { 0,0 };
		ID3D11RenderTargetView* rtvsNo[] = { NULL };
		ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, rtvsNo, NULL, 1, 2, uavs, initialCount);

		BindQuad(ImmediateContext);
		ImmediateContext->Draw(6, 0);
	}
#pragma endregion
	g_gpuProfiler.Timestamp(GTS_SortListLow, ImmediateContext);
#pragma region Min gather of alpha values
	{
		ImmediateContext->OMSetBlendState(D3D->GetBsBlendBackToFront(), blendFactor, 0xffffffff);
		ImmediateContext->ClearUnorderedAccessViewUint(Geometry->GetAlpha()[ping].GetUav(), clearUav);
		mMinGather_LowRes.SetShader(ImmediateContext);
		
		ID3D11UnorderedAccessView* uavs[] = { mStartOffsetBufferLowRes.GetUav(), mFragmentLinkBufferLowRes.GetUav(), Geometry->GetAlpha()[ping].GetUav() };
		UINT initialCount[] = { 0,0,0 };
		ID3D11RenderTargetView* rtvsNo[] = { NULL };
		ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, rtvsNo, NULL, 1, 3, uavs, initialCount);
		
		ImmediateContext->Draw(6, 0);

		{
			ID3D11UnorderedAccessView* uavs[] = { NULL, NULL, NULL, NULL };
			UINT initialCount[] = { 0,0,0 };
			ID3D11RenderTargetView* rtvsNo[] = { NULL };
			ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, rtvsNo, NULL, 1, 3, uavs, initialCount);
		}
	}
#pragma endregion
	g_gpuProfiler.Timestamp(GTS_GatherAlpha, ImmediateContext);
#pragma region Smoothing
	{
		ImmediateContext->CSSetShader(mSmoothAlpha.GetCs(), NULL, 0);

		ID3D11Buffer* cbs[] = { _CbFadeToAlpha.GetBuffer() };
		ImmediateContext->CSSetConstantBuffers(0, 1, cbs);

		for (int s = 0; s < _SmoothingIterations; ++s)
		{
			float laplaceWeight = _CbFadeToAlpha.Data.LaplaceWeight;

			for (int ta = 0; ta < 2; ++ta)
			{
				if (ta == 0) {
					_CbFadeToAlpha.Data.LaplaceWeight = laplaceWeight;
				}
				else 
				{
					_CbFadeToAlpha.Data.LaplaceWeight = -laplaceWeight * 1.01f;
				}
				_CbFadeToAlpha.UpdateBuffer(ImmediateContext);

				if (ta == 1) 
					continue;	// skip the shrinking..

				ID3D11ShaderResourceView* srvs[] = { Geometry->GetAlpha()[ping].GetSrv(), Geometry->GetSrvLineID() };
				ImmediateContext->CSSetShaderResources(0, 2, srvs);

				ID3D11UnorderedAccessView* uavs[] = { Geometry->GetAlpha()[1 - ping].GetUav() };
				UINT initialCounts[] = { 0, 0, 0, 0 };
				ImmediateContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

				UINT groupsX = Geometry->GetTotalNumberOfControlPoints();
				if (groupsX % (512) == 0)
					groupsX = groupsX / (512);
				else groupsX = groupsX / (512) + 1;
				ImmediateContext->Dispatch(groupsX, 1, 1);

				// clean up
				ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL };
				ImmediateContext->CSSetShaderResources(0, 2, noSrvs);

				ID3D11UnorderedAccessView* noUavs[] = { NULL };
				ImmediateContext->CSSetUnorderedAccessViews(0, 1, noUavs, initialCounts);

				ping = 1 - ping;	// ping pong!
			}
			_CbFadeToAlpha.Data.LaplaceWeight = laplaceWeight;
		}

		ID3D11Buffer* noCbs[] = { NULL };
		ImmediateContext->CSSetConstantBuffers(0, 1, noCbs);
	}
#pragma endregion
	g_gpuProfiler.Timestamp(GTS_Smoothing, ImmediateContext);
#pragma region Fade the current alpha solution per vertex
	{
		ImmediateContext->CSSetShader(mFadeAlpha.GetCs(), NULL, 0);

		ID3D11ShaderResourceView* srvs[] = { Geometry->GetAlpha()[ping].GetSrv(), Geometry->GetSrvAlphaWeights() };
		ImmediateContext->CSSetShaderResources(0, 2, srvs);

		ID3D11UnorderedAccessView* uavs[] = { Geometry->GetCurrentAlpha().GetUav() };
		UINT initialCounts[] = { 0,0,0,0 };
		ImmediateContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

		ID3D11Buffer* cbs[] = { _CbFadeToAlpha.GetBuffer() };
		ImmediateContext->CSSetConstantBuffers(0, 1, cbs);

		UINT groupsX = Geometry->GetTotalNumberOfVertices();
		if (groupsX % (512) == 0)
			groupsX = groupsX / (512);
		else groupsX = groupsX / (512) + 1;
		ImmediateContext->Dispatch(groupsX, 1, 1);

		// clean up
		ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL };
		ImmediateContext->CSSetShaderResources(0, 2, noSrvs);

		ID3D11UnorderedAccessView* noUavs[] = { NULL };
		ImmediateContext->CSSetUnorderedAccessViews(0, 1, noUavs, initialCounts);

		ID3D11Buffer* noCbs[] = { NULL };
		ImmediateContext->CSSetConstantBuffers(0, 1, noCbs);
	}
#pragma endregion
	g_gpuProfiler.Timestamp(GTS_FadeAlpha, ImmediateContext);

	// switch to full viewport
	ImmediateContext->RSSetViewports(1, &fullViewport);

#pragma region Create fragment linked list
	{
		// Clear the start offset buffer by magic value.
		ImmediateContext->ClearUnorderedAccessViewUint(mStartOffsetBuffer.GetUav(), clearUav);
		// Bind states
		ID3D11RenderTargetView* rtvsNo[] = { NULL };
		ImmediateContext->OMSetRenderTargets(0, rtvsNo, NULL);
		ImmediateContext->OMSetBlendState(D3D->GetBsDefault(), blendFactor, 0xffffffff);
		ImmediateContext->OMSetDepthStencilState(D3D->GetDsTestWriteOff(), 0);
		ImmediateContext->RSSetState(D3D->GetRsCullNone());

		ImmediateContext->IASetInputLayout(_InputLayout_Line_HQ);

		mLineShader_HQ.SetShader(ImmediateContext);

		ID3D11ShaderResourceView* srvs[] = { D3D->GetSrvDepthbuffer() };
		ImmediateContext->PSSetShaderResources(0, 1, srvs);

		{
			ID3D11UnorderedAccessView* uavs[] = { mFragmentLinkBuffer.GetUav(), mStartOffsetBuffer.GetUav() };
			UINT initialCount[] = { 0,0 };
			// render target is only bound so that the rasterizer knows, how many samples we want. We won't render into it, yet.
			ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, rtvs, NULL, 1, 2, uavs, initialCount);
		}

		// Render
		Geometry->DrawHQ(ImmediateContext);

		ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL, NULL, NULL };
		ImmediateContext->GSSetShaderResources(0, 4, noSrvs);
		ImmediateContext->PSSetShaderResources(0, 4, noSrvs);
	}
#pragma endregion
	g_gpuProfiler.Timestamp(GTS_CreateList, ImmediateContext);
#pragma region Sort the fragments
	{
		mSortFragments.SetShader(ImmediateContext);
		ID3D11UnorderedAccessView* uavs[] = { mStartOffsetBuffer.GetUav(), mFragmentLinkBuffer.GetUav() };
		UINT initialCount[] = { 0,-1 };
		ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, rtvs, D3D->GetDsvBackbuffer(), 1, 2, uavs, initialCount);
		BindQuad(ImmediateContext);
		ImmediateContext->Draw(6, 0);
	}
#pragma endregion
	g_gpuProfiler.Timestamp(GTS_SortList, ImmediateContext);
#pragma region Calculate the histogram
	{
		if (mHistogramMode != HistogramMode::None)
		{
			ID3D11RenderTargetView* rtv = mHistogramTarget.GetRtv();
			ImmediateContext->RSSetViewports(1, &binViewport);
			ImmediateContext->OMSetRenderTargets(1, &rtv, NULL);
			ImmediateContext->OMSetBlendState(D3D->GetBsAdditive(), blendFactor, 0xffffffff);
			ImmediateContext->OMSetDepthStencilState(D3D->GetDsTestWriteOff(), 0);				// already bound
			ImmediateContext->RSSetState(D3D->GetRsCullNone());									// already bound

			float clearColor[4] = { 0.001f, 0.001f, 0.001f, 0.001f }; // set to small value -> set to zero leads to bad behaviour when no fragments are on the screen
			ImmediateContext->ClearRenderTargetView(rtv, clearColor);

			ID3D11Buffer* vbs[] = { NULL, NULL, NULL, NULL };
			UINT strides[] = { 0, 0, 0, 0 };
			UINT offsets[] = { 0, 0, 0, 0 };
			ImmediateContext->IASetVertexBuffers(0, 4, vbs, strides, offsets);

			ImmediateContext->IASetInputLayout(NULL);
			ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

			mCalculateHistogram.SetShader(ImmediateContext);
			ID3D11ShaderResourceView* srvs[] = { mFragmentLinkBuffer.GetSrv() };
			ImmediateContext->VSSetShaderResources(0, 1, srvs);
			ImmediateContext->PSSetShaderResources(0, 1, srvs);

			ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &rtv, NULL, 1, 0, NULL, NULL);

			ImmediateContext->CopyStructureCount(mFragmentCounterBuffer.GetBuf(), 0, mFragmentLinkBuffer.GetUav());
			ImmediateContext->DrawInstancedIndirect(mFragmentCounterBuffer.GetBuf(), 0);

			ID3D11RenderTargetView* rtvsNo[] = { NULL };
			ImmediateContext->OMSetRenderTargets(0, rtvsNo, NULL);

			ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL, NULL };
			ImmediateContext->VSSetShaderResources(0, 3, noSrvs);
			ImmediateContext->PSSetShaderResources(0, 3, noSrvs);

			ImmediateContext->RSSetViewports(1, &fullViewport);
		}
	}
#pragma endregion
	g_gpuProfiler.Timestamp(GTS_Histogram, ImmediateContext);
#pragma region Calculate the normalized Histogram CDF
	{
		switch (mHistogramMode)
		{
		case HistogramMode::None:
			break;
		case HistogramMode::ScalarColor:
			break;
		case HistogramMode::Equalization:
			CalculateHistogramCDF(ImmediateContext);
			break;
		case HistogramMode::Bi_Equalization:
			CalculateBiHistogramCDF(ImmediateContext);
			break;
		case HistogramMode::SegmentedEqualization:
			CalculateSegmentedHistogramCDF(ImmediateContext);
			break;
		}
	}
#pragma endregion
	g_gpuProfiler.Timestamp(GTS_HistogramCDF, ImmediateContext);
#pragma region Calculate the Histogram CDF Exponential Damping
	{
		ImmediateContext->CSSetShader(mExponentialDamping.GetCs(), NULL, 0);

		ID3D11Buffer* cbs[] = { _CbRenderer.GetBuffer() };
		ImmediateContext->CSSetConstantBuffers(1, 1, cbs);

		ID3D11ShaderResourceView* srvs[] = { mHistogramCDF.GetSrv() };
		ImmediateContext->CSSetShaderResources(0, 1, srvs);

		ID3D11UnorderedAccessView* uavs[] = { mHistogramCDFExponentialDamping.GetUav() };
		UINT initialCounts[] = { 0 };
		ImmediateContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

		UINT groupsX = HISTOGRAM_BINS;
		ImmediateContext->Dispatch(groupsX, 1, 1);

		// clean up
		ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL };
		ImmediateContext->CSSetShaderResources(0, 2, noSrvs);

		ID3D11UnorderedAccessView* noUavs[] = { NULL };
		ImmediateContext->CSSetUnorderedAccessViews(0, 1, noUavs, initialCounts);

		ID3D11Buffer* noCbs[] = { NULL, NULL };
		ImmediateContext->CSSetConstantBuffers(0, 2, noCbs);
	}
#pragma endregion
	g_gpuProfiler.Timestamp(GTS_ExponentialDamping, ImmediateContext);
#pragma region Render the fragments with Histogram Equalization
	{
		ImmediateContext->OMSetBlendState(D3D->GetBsBlendBackToFront(), blendFactor, 0xffffffff); // can this be removed -> depends on the shader

		mRenderFragments.SetShader(ImmediateContext);
		ID3D11ShaderResourceView* srvs[] = { mHistogramCDFExponentialDamping.GetSrv(), g_Colormap->GetColormapTexture().GetSrv(), mHistogramTarget.GetSrv() };
		ImmediateContext->VSSetShaderResources(0, 3, srvs);
		ImmediateContext->PSSetShaderResources(0, 3, srvs);

		ID3D11UnorderedAccessView* uavs[] = { mStartOffsetBuffer.GetUav(), mFragmentLinkBuffer.GetUav() };
		UINT initialCount[] = { 0,-1 };
		ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, rtvs, D3D->GetDsvBackbuffer(), 1, 2, uavs, initialCount);

		ImmediateContext->PSSetSamplers(0, 1, &mSamplerState);

		BindQuad(ImmediateContext);
		ImmediateContext->Draw(6, 0);

		ImmediateContext->OMSetRenderTargets(1, rtvs, D3D->GetDsvBackbuffer());

		ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL, NULL };
		ImmediateContext->VSSetShaderResources(0, 3, noSrvs);
		ImmediateContext->PSSetShaderResources(0, 3, noSrvs);
	}
#pragma endregion
	g_gpuProfiler.Timestamp(GTS_RenderImage, ImmediateContext);

}

// three times the same equation -> simplfy
void Renderer::CalculateHistogramCDF(ID3D11DeviceContext* ImmediateContext)
{
	ImmediateContext->CSSetShader(mCalculateHistogramCDF.GetCs(), NULL, 0);

	ID3D11ShaderResourceView* srvs[] = { mHistogramTarget.GetSrv() };
	ImmediateContext->CSSetShaderResources(0, 1, srvs);

	ID3D11UnorderedAccessView* uavs[] = { mHistogramCDF.GetUav() };
	UINT initialCounts[] = { 0 };
	ImmediateContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

	UINT groupsX = 1;
	ImmediateContext->Dispatch(groupsX, 1, 1);

	// clean up
	ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL };
	ImmediateContext->CSSetShaderResources(0, 2, noSrvs);

	ID3D11UnorderedAccessView* noUavs[] = { NULL };
	ImmediateContext->CSSetUnorderedAccessViews(0, 1, noUavs, initialCounts);
}

void Renderer::CalculateBiHistogramCDF(ID3D11DeviceContext* ImmediateContext)
{
	ImmediateContext->CSSetShader(mCalculateBiHistogramCDF.GetCs(), NULL, 0);

	ID3D11ShaderResourceView* srvs[] = { mHistogramTarget.GetSrv() };
	ImmediateContext->CSSetShaderResources(0, 1, srvs);

	ID3D11UnorderedAccessView* uavs[] = { mHistogramCDF.GetUav() };
	UINT initialCounts[] = { 0 };
	ImmediateContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

	ImmediateContext->Dispatch(1, 1, 1);

	// clean up
	ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL };
	ImmediateContext->CSSetShaderResources(0, 2, noSrvs);

	ID3D11UnorderedAccessView* noUavs[] = { NULL };
	ImmediateContext->CSSetUnorderedAccessViews(0, 1, noUavs, initialCounts);
}

void Renderer::CalculateSegmentedHistogramCDF(ID3D11DeviceContext* ImmediateContext)
{
	ImmediateContext->CSSetShader(mCalculateSegmentedHistogramCDF.GetCs(), NULL, 0);

	ID3D11ShaderResourceView* srvs[] = { mHistogramTarget.GetSrv() };
	ImmediateContext->CSSetShaderResources(0, 1, srvs);

	ID3D11UnorderedAccessView* uavs[] = { mHistogramCDF.GetUav() };
	UINT initialCounts[] = { 0 };
	ImmediateContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);


	ID3D11Buffer* cbs[] = { _CbRenderer.GetBuffer() };
	ImmediateContext->CSSetConstantBuffers(1, 1, cbs);

	ImmediateContext->Dispatch(1, 1, 1);

	// clean up
	ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL };
	ImmediateContext->CSSetShaderResources(0, 2, noSrvs);

	ID3D11UnorderedAccessView* noUavs[] = { NULL };
	ImmediateContext->CSSetUnorderedAccessViews(0, 1, noUavs, initialCounts);
}

// only used in imgui -> to show the Normalized Histogram
void Renderer::CalculateNormalizedHistogram(ID3D11DeviceContext* ImmediateContext, D3D* D3D)
{
	const D3D11_VIEWPORT& fullViewport = D3D->GetFullViewport();

	D3D11_VIEWPORT binViewport = fullViewport;
	binViewport.Width = HISTOGRAM_BINS;
	binViewport.Height = 1;

	ID3D11RenderTargetView* rtvs[] = { D3D->GetRtvBackbuffer() };
	float blendFactor[4] = { 1,1,1,1 };


#pragma region Replace Scalar Values with normalized scalar values
	// -------------------------------------------
	{
		mReplaceScalarColor.SetShader(ImmediateContext);
		ID3D11ShaderResourceView* srvs[] = { mHistogramCDF.GetSrv(), mHistogramTarget.GetSrv() };
		ImmediateContext->VSSetShaderResources(0, 2, srvs);
		ImmediateContext->PSSetShaderResources(0, 2, srvs);

		ID3D11UnorderedAccessView* uavs[] = { mStartOffsetBuffer.GetUav(), mFragmentLinkBuffer.GetUav() };
		UINT initialCount[] = { 0, -1 };
		ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, rtvs, NULL, 1, 2, uavs, initialCount);
		BindQuad(ImmediateContext);
		ImmediateContext->Draw(6, 0);

		ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL, NULL };
		ImmediateContext->VSSetShaderResources(0, 2, noSrvs);
		ImmediateContext->PSSetShaderResources(0, 2, noSrvs);
	}
#pragma endregion
	// -------------------------------------------
	// 
	// -------------------------------------------
#pragma region Calculate the NORMALIZED histogram
	{
		ID3D11RenderTargetView* rtv = mHistogramTargetNormalized.GetRtv();
		ImmediateContext->RSSetViewports(1, &binViewport);
		ImmediateContext->OMSetRenderTargets(1, &rtv, NULL);
		ImmediateContext->OMSetBlendState(D3D->GetBsAdditive(), blendFactor, 0xffffffff);
		ImmediateContext->OMSetDepthStencilState(D3D->GetDsTestWriteOff(), 0);				// already bound
		ImmediateContext->RSSetState(D3D->GetRsCullNone());									// already bound

		float clearColor[4] = { 0.f, 0.f, 0.f, 0.f };
		ImmediateContext->ClearRenderTargetView(rtv, clearColor);

		ID3D11Buffer* vbs[] = { NULL, NULL, NULL, NULL };
		UINT strides[] = { 0, 0, 0, 0 };
		UINT offsets[] = { 0, 0, 0, 0 };
		ImmediateContext->IASetVertexBuffers(0, 4, vbs, strides, offsets);

		ImmediateContext->IASetInputLayout(NULL);
		ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);

		mCalculateHistogram.SetShader(ImmediateContext);
		ID3D11ShaderResourceView* srvs[] = { mFragmentLinkBuffer.GetSrv() };
		ImmediateContext->VSSetShaderResources(0, 1, srvs);
		ImmediateContext->PSSetShaderResources(0, 1, srvs);

		ImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &rtv, NULL, 1, 0, NULL, NULL);

		ImmediateContext->CopyStructureCount(mFragmentCounterBuffer.GetBuf(), 0, mFragmentLinkBuffer.GetUav());
		ImmediateContext->DrawInstancedIndirect(mFragmentCounterBuffer.GetBuf(), 0);

		ID3D11RenderTargetView* rtvsNo[] = { NULL };
		ImmediateContext->OMSetRenderTargets(0, rtvsNo, NULL);

		ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL, NULL };
		ImmediateContext->VSSetShaderResources(0, 3, noSrvs);
		ImmediateContext->PSSetShaderResources(0, 3, noSrvs);

		ImmediateContext->RSSetViewports(1, &fullViewport);
	}
	
#pragma endregion
	// -------------------------------------------
	//
	// -------------------------------------------
#pragma region Calculate the NORMALIZED normalized Histogram CDF
	{
		ImmediateContext->CSSetShader(mCalculateHistogramCDF.GetCs(), NULL, 0);

		ID3D11ShaderResourceView* srvs[] = { mHistogramTargetNormalized.GetSrv() };
		ImmediateContext->CSSetShaderResources(0, 1, srvs);

		ID3D11UnorderedAccessView* uavs[] = { mHistogramCDFNormalized.GetUav() };
		UINT initialCounts[] = { 0 };
		ImmediateContext->CSSetUnorderedAccessViews(0, 1, uavs, initialCounts);

		UINT groupsX = 1;
		ImmediateContext->Dispatch(groupsX, 1, 1);

		// clean up
		ID3D11ShaderResourceView* noSrvs[] = { NULL, NULL };
		ImmediateContext->CSSetShaderResources(0, 2, noSrvs);

		ID3D11UnorderedAccessView* noUavs[] = { NULL };
		ImmediateContext->CSSetUnorderedAccessViews(0, 1, noUavs, initialCounts);

		ID3D11Buffer* noCbs[] = { NULL };
		ImmediateContext->CSSetConstantBuffers(0, 1, noCbs);
	}
#pragma endregion
}

void Renderer::ReadTexture(ID3D11DeviceContext* ImmediateContext)
{
	mHistogramTarget.ReadTexture(ImmediateContext);
	//mHistogramTarget.CopyToCpu(ImmediateContext);
}