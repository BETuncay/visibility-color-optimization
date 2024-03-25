#pragma once

#include <d3d11.h>
#include "d3d.hpp"
#include "camera.hpp"
#include "lines.hpp"
#include "cbuffer.hpp"
#include "rendertarget2d.hpp"
#include "buffer.hpp"
#include "shader.hpp"
#include "colormap.hpp"

class Renderer
{
public:

	enum class HistogramMode {
		None = 0,
		Equalization = 1,
		Bi_Equalization = 2,
		ScalarColor = 3,
		SegmentedEqualization = 4
	};

	// The fragment linked lists draw fragments from a
	// memory pool. The size of the pool is determined by
	// an expected average overdraw rate.
	static const int EXPECTED_OVERDRAW_IN_LINKED_LISTS = 16;

	//static const int HISTOGRAM_BINS = 64;

	// size of structured buffer should be multiple of 16 bytes
	// https://developer.nvidia.com/content/understanding-structured-buffer-performance
	struct FragmentData {
		unsigned int Color;		// Pixel color
		unsigned int Depth;		// Depth
		unsigned int Coverage;	// Coverage
		float ScalarColor;
		float Transparency;
		float Transmittance;
		unsigned int Pad;
		static int GetSizeInBytes() { return sizeof(unsigned int) * 4 + sizeof(float) * 3; }
	};

	struct FragmentDataLowRes {
		unsigned int Depth;		// Depth
		float AlphaWeight;		// Alpha weight
		float Importance;
		static int GetSizeInBytes() { return sizeof(unsigned int) * 1 + sizeof(float) * 2; }
	};

	struct FragmentLink {
		FragmentData FragmentData;	// Fragment data
		unsigned int Next;			// Link to next fragment
		static int GetSizeInBytes() { return sizeof(unsigned int) + FragmentData::GetSizeInBytes(); }
	};

	struct FragmentLinkLowRes {
		FragmentDataLowRes FragmentData;	// Fragment data
		unsigned int Next;					// Link to next fragment

		static int GetSizeInBytes() { return sizeof(unsigned int) * 4; }
	};

	struct CbFadeToAlpha
	{
		CbFadeToAlpha() : FadeToAlpha(0.1f), LaplaceWeight(0.1f) {}
		float FadeToAlpha;
		float LaplaceWeight;
	};

	struct CbRenderer
	{
		CbRenderer() : Q(0), R(0), Lambda(1), TotalNumberOfControlPoints(1),
			LineColor(1, 163.0f / 255.0f, 0, 1),
			HaloColor(0, 0, 0, 1),
			StripWidth(0.00015f),
			HaloPortion(0.7f),
			ScreenWidth(100),
			ScreenHeight(100),
			dt(0.01f),
			DampingHalflife(0.8),
			HistogramMode(HistogramMode::Equalization),
			paddingForTheArray(0.f)
			{}
		float Q;
		float R;
		float Lambda;
		int TotalNumberOfControlPoints;
		XMFLOAT4 LineColor;
		XMFLOAT4 HaloColor;
		float StripWidth;
		float HaloPortion;
		int ScreenWidth;
		int ScreenHeight;
		float dt;
		float DampingHalflife;
		HistogramMode HistogramMode;
		float paddingForTheArray;
		float HistogramSegments[HISTOGRAM_SEGMENTS_MAX];
	};

	Renderer(float q, float r, float lambda, float stripWidth, int smoothingIterations, HistogramMode histogramMode, std::vector<float> histogramSegments, ID3D11Device* device, const DXGI_SURFACE_DESC* BackBufferSurfaceDesc);
	~Renderer();

	bool D3DCreateDevice(ID3D11Device* Device);
	bool D3DCreateSwapChain(ID3D11Device* Device, const DXGI_SURFACE_DESC* BackBufferSurfaceDesc);
	void D3DReleaseDevice();
	void D3DReleaseSwapChain();
	void InitBufferData(ID3D11DeviceContext* ImmediateContext);
	void BindQuad(ID3D11DeviceContext* ImmediateContext);
	void Draw(ID3D11DeviceContext* ImmediateContext, D3D* D3D, Lines* Geometry, Camera* Camera, Colormap* g_Colormap, float dt);
	void ReadTexture(ID3D11DeviceContext* ImmediateContext);
	void CalculateNormalizedHistogram(ID3D11DeviceContext* ImmediateContext, D3D* D3D);
	void CalculateHistogramCDF(ID3D11DeviceContext* ImmediateContext);
	void CalculateBiHistogramCDF(ID3D11DeviceContext* ImmediateContext);
	void CalculateSegmentedHistogramCDF(ID3D11DeviceContext* ImmediateContext);


	vislab::RenderTarget2dD3D11& GetHistogram() { return mHistogramTarget; }
	vislab::BufferD3D11& GetHistogramCDF() { return mHistogramCDF; }
	vislab::RenderTarget2dD3D11& GetHistogramNormalized() { return mHistogramTargetNormalized; }
	vislab::BufferD3D11& GetHistogramCDFExponentialDamping() { return mHistogramCDFExponentialDamping; }
	vislab::BufferD3D11& GetHistogramCDFNormalized() { return mHistogramCDFNormalized; }


	float GetDampingHalflife() { return mDampingHalflife; }
	void SetDampingHalflife(float value) 
	{ 
		mDampingHalflife = value; 
		_CbRenderer.Data.DampingHalflife = mDampingHalflife;
	}
	HistogramMode GetHistogramMode() { return _CbRenderer.Data.HistogramMode; }
	void SetHistogramMode(HistogramMode value)
	{ 
		mHistogramMode = value;
		_CbRenderer.Data.HistogramMode = value;
	}

	ConstantBuffer<CbRenderer> _CbRenderer;
private:
	int _ResolutionDownScale;
	int _SmoothingIterations;
	float mDampingHalflife;
	HistogramMode mHistogramMode;

	vislab::BufferD3D11 mStartOffsetBuffer;
	vislab::BufferD3D11 mStartOffsetBufferLowRes;
	vislab::BufferD3D11 mFragmentLinkBuffer;
	vislab::BufferD3D11 mFragmentLinkBufferLowRes;
	vislab::BufferD3D11 mFragmentCounterBuffer;
	vislab::BufferD3D11 mHistogramCDF;
	vislab::BufferD3D11 mHistogramCDFPrev; // contains CDF of previous frame
	vislab::BufferD3D11 mHistogramCDFExponentialDamping;
	vislab::BufferD3D11 mHistogramCDFNormalized; // for imgui -> cdf of the histogram with normalized scalar values

	vislab::RenderTarget2dD3D11 mHistogramTarget;
	vislab::RenderTarget2dD3D11 mHistogramTargetNormalized; //for imgui -> histogram with normalized scalar values


	ID3D11Buffer* _VbViewportQuad;

	ID3D11InputLayout* _InputLayout_Line_HQ;
	ID3D11InputLayout* _InputLayout_Line_LowRes;
	ID3D11InputLayout* _InputLayout_ViewportQuad;

	ConstantBuffer<CbFadeToAlpha> _CbFadeToAlpha;
	//ConstantBuffer<CbRenderer> _CbRenderer;

	vislab::ShaderD3D11 mLineShader_HQ;
	vislab::ShaderD3D11 mLineShader_LowRes;
	vislab::ShaderD3D11 mMinGather_LowRes;
	vislab::ShaderD3D11 mRenderFragments;
	vislab::ShaderD3D11 mSortFragments;
	vislab::ShaderD3D11 mSortFragments_LowRes;
	vislab::ShaderD3D11 mCalculateHistogram;
	vislab::ShaderD3D11 mDebugQuad;
	vislab::ShaderD3D11 mDebugGeometry;
	vislab::ShaderD3D11 mFadeAlpha;
	vislab::ShaderD3D11 mSmoothAlpha;
	vislab::ShaderD3D11 mCalculateHistogramCDF;
	vislab::ShaderD3D11 mCalculateBiHistogramCDF;
	vislab::ShaderD3D11 mCalculateSegmentedHistogramCDF;

	vislab::ShaderD3D11 mReplaceScalarColor;
	vislab::ShaderD3D11 mExponentialDamping;

	ID3D11SamplerState* mSamplerState;
};