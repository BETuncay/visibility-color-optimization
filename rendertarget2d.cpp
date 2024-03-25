#include "rendertarget2d.hpp"
#include <d3d11.h>

#define SAFE_RELEASE(x) if (x) x->Release(); x = NULL;

namespace vislab
{
	RenderTarget2dD3D11::RenderTarget2dD3D11(const TargetDesc& desc) :
		Desc(desc),
		mTex(NULL),
		mSrv(NULL),
		mRtv(NULL),
		mStaging(NULL),
		mDynamic(NULL)
	{
		if (desc.AccessFlag != 0)
			Data.resize(desc.Resolution.x() * desc.Resolution.y());
	}

	RenderTarget2dD3D11::RenderTarget2dD3D11(const RenderTarget2dD3D11& other) :
		Desc(other.Desc),
		mTex(NULL),
		mSrv(NULL),
		mRtv(NULL),
		mStaging(NULL),
		mDynamic(NULL),
		Data(other.Data)
	{
	}

	RenderTarget2dD3D11::~RenderTarget2dD3D11()
	{
		D3DRelease();
	}

	void RenderTarget2dD3D11::Clear(ID3D11DeviceContext* immediateContext, const Eigen::Vector4f& clearColor)
	{
		immediateContext->ClearRenderTargetView(mRtv, clearColor.data());
	}

	bool RenderTarget2dD3D11::D3DCreate(ID3D11Device* Device, D3D11_SUBRESOURCE_DATA* pInit)
	{
		// create the 3D texture
		D3D11_TEXTURE2D_DESC texDesc;
		ZeroMemory(&texDesc, sizeof(D3D11_TEXTURE2D_DESC));
		texDesc.Width = Desc.Resolution.x();
		texDesc.Height = Desc.Resolution.y();
		texDesc.ArraySize = 1;
		texDesc.MipLevels = 1;
		texDesc.BindFlags = Desc.BindFlags;
		texDesc.Format = Desc.Format;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.CPUAccessFlags = 0;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;

		D3D11_SUBRESOURCE_DATA init;
		ZeroMemory(&init, sizeof(D3D11_SUBRESOURCE_DATA));
		if (pInit != NULL) 
		{
			init = *pInit;
		}
		else
		{
			std::vector<Eigen::Vector4f> initialData(texDesc.Width * texDesc.Height, Eigen::Vector4f(1, 0.5, 0, 1));
			init.pSysMem = initialData.data();
			init.SysMemPitch = sizeof(Eigen::Vector4f) * Desc.Resolution.x();
			init.SysMemSlicePitch = sizeof(Eigen::Vector4f) * Desc.Resolution.x() * Desc.Resolution.y();
		}
		if (FAILED(Device->CreateTexture2D(&texDesc, &init, &mTex))) {
			return false;
		}
		// create the shader resource buffer
		if ((Desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0)
		{
			if (FAILED(Device->CreateShaderResourceView(mTex, NULL, &mSrv))) {
				return false;
			}
		}
		// create the render target view
		if ((Desc.BindFlags & D3D11_BIND_RENDER_TARGET) != 0)
		{
			if (FAILED(Device->CreateRenderTargetView(mTex, NULL, &mRtv))) {
				return false;
			}
		}
		// create the staging buffer
		if ((Desc.AccessFlag & D3D11_CPU_ACCESS_READ) != 0)
		{
			texDesc.BindFlags = 0;
			texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			texDesc.Usage = D3D11_USAGE_STAGING;
			if (FAILED(Device->CreateTexture2D(&texDesc, &init, &mStaging))) {
				return false;
			}
		}
		// create the dynamic buffer
		if ((Desc.AccessFlag & D3D11_CPU_ACCESS_WRITE) != 0)
		{
			texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			texDesc.Usage = D3D11_USAGE_DYNAMIC;
			if (FAILED(Device->CreateTexture2D(&texDesc, &init, &mDynamic))) {
				return false;
			}
		}
		return true;
	}

	void RenderTarget2dD3D11::D3DRelease()
	{
		SAFE_RELEASE(mTex);
		SAFE_RELEASE(mSrv);
		SAFE_RELEASE(mRtv);
		SAFE_RELEASE(mStaging);
		SAFE_RELEASE(mDynamic);
	}

	void RenderTarget2dD3D11::CopyToCpu(ID3D11DeviceContext* immediateContext)
	{
		if ((Desc.AccessFlag & D3D11_CPU_ACCESS_READ) == 0) return;

		immediateContext->CopyResource(mStaging, mTex);

		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(immediateContext->Map(mStaging, 0, D3D11_MAP_READ, 0, &mapped)))
		{
			BYTE* mappedData = reinterpret_cast<BYTE*>(mapped.pData);	// destination
			BYTE* buffer = (BYTE*)&(Data[0]);							// source
			int rowspan = Desc.Resolution.x() * GetFormatSizeInByte();	// row-pitch in source
			for (UINT iy = 0; iy < Desc.Resolution.y(); ++iy)			// copy in the data line by line
			{
				memcpy(
					buffer + rowspan * iy,
					mappedData + mapped.RowPitch * iy,
					rowspan);
			}
			immediateContext->Unmap(mStaging, 0);
		}
	}

	void RenderTarget2dD3D11::CopyToGpu(ID3D11DeviceContext* immediateContext)
	{
		return; 
		if ((Desc.AccessFlag & D3D11_CPU_ACCESS_WRITE) == 0) return;

		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(immediateContext->Map(mDynamic, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			BYTE* mappedData = reinterpret_cast<BYTE*>(mapped.pData); // destination
			BYTE* buffer = (BYTE*)&(Data[0]);		// source
			int rowspan = Desc.Resolution.x() * GetFormatSizeInByte();	// row-pitch in source
			for (UINT iy = 0; iy < Desc.Resolution.y(); ++iy)		// copy in the data line by line
			{
				memcpy(
					buffer + rowspan * iy,
					mappedData + mapped.RowPitch * iy,
					rowspan);
			}
			immediateContext->Unmap(mDynamic, 0);
		}
		immediateContext->CopyResource(mTex, mDynamic);
	}

	unsigned int RenderTarget2dD3D11::GetFormatSizeInByte()
	{
		switch (Desc.Format)
		{
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
			return sizeof(float) * 4;
		case DXGI_FORMAT_R32G32B32_FLOAT:
			return sizeof(float) * 3;
		case DXGI_FORMAT_R32G32_FLOAT:
			return sizeof(float) * 2;
		case DXGI_FORMAT_R32_FLOAT:
			return sizeof(float) * 1;
		default:
			throw std::exception("Format not implemented in RenderTarget2dD3D11!\n");
		}
		return sizeof(float);
	}

	void RenderTarget2dD3D11::ReadTexture(ID3D11DeviceContext* ImmediateContext)
	{
		ImmediateContext->CopyResource(mStaging, mTex);

		FLOAT* Read = new FLOAT[Desc.Resolution[0] * Desc.Resolution[1]]();
		for (int i = 0; i < Desc.Resolution[0]; i++)
		{
			for (int j = 0; j < Desc.Resolution[1]; ++j)
			{
				Read[i + j * Desc.Resolution[0]] = -1.f;
			}
		}


		D3D11_MAPPED_SUBRESOURCE ResourceDesc = {};
		ImmediateContext->Map(mStaging, 0, D3D11_MAP_READ, 0, &ResourceDesc);

		if (ResourceDesc.pData)
		{
			int const BytesPerPixel = sizeof(FLOAT);
			for (int i = 0; i < Desc.Resolution[1]; ++i)
			{
				std::memcpy(
					(byte*) Read + Desc.Resolution[0] * BytesPerPixel * i,
					(byte*)ResourceDesc.pData + ResourceDesc.RowPitch * i,
					Desc.Resolution[0] * BytesPerPixel);
			}
		}

		ImmediateContext->Unmap(mStaging, 0);

		printf("\n");
		for (int i = 0; i < Desc.Resolution[0]; ++i)
		{
			for (int j = 0; j < Desc.Resolution[1]; ++j)
			{
				printf("[%d, %d] = { %.6f }\n", i, j, Read[i + j * Desc.Resolution[0]]);
			}
		}

		delete[] Read;
	}
}