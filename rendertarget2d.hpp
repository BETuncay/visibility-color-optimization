#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <d3d11_1.h>
#include <Eigen/eigen>


namespace vislab
{
	// A class that manages a 2D render target.
	class RenderTarget2dD3D11
	{
	public:
		// Defines all properties of the D3D resource.
		struct TargetDesc
		{
			TargetDesc() : AccessFlag((D3D11_CPU_ACCESS_FLAG)0), BindFlags((D3D11_BIND_FLAG)0), Resolution(4, 4), Format(DXGI_FORMAT_R32G32B32A32_FLOAT) {}
			TargetDesc(const Eigen::Vector2i& resolution, const DXGI_FORMAT& format, int bindFlags, int accessFlag = 0) : Resolution(resolution), Format(format), AccessFlag(accessFlag), BindFlags(bindFlags) {}

			Eigen::Vector2i Resolution;
			DXGI_FORMAT Format;
			int AccessFlag;
			int BindFlags;
		};

		// Constructor.
		RenderTarget2dD3D11(const TargetDesc& desc = TargetDesc());
		// Copy-Constructor.
		RenderTarget2dD3D11(const RenderTarget2dD3D11& other);
		// Destructor.
		~RenderTarget2dD3D11();

		// Clears the render target view.
		void Clear(ID3D11DeviceContext* immediateContext, const Eigen::Vector4f& clearColor);

		// Creates the D3D resources on the GPU.
		bool D3DCreate(ID3D11Device* Device, D3D11_SUBRESOURCE_DATA* pInit = NULL);
		// Releases the D3D resources from the GPU.
		void D3DRelease();

		// Transfers data from the GPU to the CPU.
		void CopyToCpu(ID3D11DeviceContext* immediateContext);
		// Transfers data from the CPU to the GPU.
		void CopyToGpu(ID3D11DeviceContext* immediateContext);

		// print content of texture to console -> temporary function until I understand CopyToCpu()
		void RenderTarget2dD3D11::ReadTexture(ID3D11DeviceContext* ImmediateContext);

		// Gets the underlying texture.
		ID3D11Texture2D* GetTex() { return mTex; }
		// Gets the shader resource view for read-only access in shaders.
		ID3D11ShaderResourceView* GetSrv() { return mSrv; }
		// Gets the render target view for write-only access in pixel shaders.
		ID3D11RenderTargetView* GetRtv() { return mRtv; }
		// Gets the internal format of the texture.
		const DXGI_FORMAT& GetFormat() const { return Desc.Format; }
		// Gets the resolution of the texture.
		const Eigen::Vector2i& GetResolution() const { return Desc.Resolution; }

		// Vector containing the data stored in this texture.
		std::vector<float> Data;

		// Configuration of the D3D resources.
		TargetDesc Desc;

	private:
		// Calculates the size in bytes for a specific format
		unsigned int GetFormatSizeInByte();

		ID3D11Texture2D* mTex;				// GPU resource.
		ID3D11Texture2D* mDynamic;			// GPU resource for writing to the GPU.
		ID3D11Texture2D* mStaging;			// GPU resource for reading from the GPU.
		ID3D11ShaderResourceView* mSrv;		// Shader resource view for read-only access in shaders.
		ID3D11RenderTargetView* mRtv;		// Render target view for write-only access in pixel shaders.
	};
}