#include "buffer.hpp"


#define SAFE_RELEASE(x) if (x) x->Release(); x = NULL;

namespace vislab
{
	BufferD3D11::BufferD3D11(const BufferDesc& desc) :
		mBuf(NULL),
		mUav(NULL),
		mSrv(NULL),
		mStaging(NULL),
		mDesc(desc)
	{
		if (desc.CPUAccessFlag != 0)
			mData.resize(desc.Num_Elements * desc.Size_Element);
	}

	BufferD3D11::BufferD3D11(const BufferD3D11& other) :
		mBuf(NULL),
		mUav(NULL),
		mSrv(NULL),
		mStaging(NULL),
		mDesc(other.mDesc)
	{}

	BufferD3D11::~BufferD3D11()
	{
		D3DRelease();
	}

	bool BufferD3D11::D3DCreate(ID3D11Device* Device)
	{
		D3D11_BUFFER_DESC bufDesc;
		ZeroMemory(&bufDesc, sizeof(D3D11_BUFFER_DESC));
		bufDesc.BindFlags = mDesc.BindFlags;
		bufDesc.ByteWidth = mDesc.Num_Elements * mDesc.Size_Element;
		bufDesc.CPUAccessFlags = 0;
		bufDesc.MiscFlags = mDesc.MiscFlags;
		bufDesc.StructureByteStride = mDesc.Size_Element;
		bufDesc.Usage = D3D11_USAGE_DEFAULT;
		if (FAILED(Device->CreateBuffer(&bufDesc, NULL, &mBuf))) return false;

		if (mDesc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
			ZeroMemory(&uavDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
			uavDesc.Buffer.FirstElement = 0;
			uavDesc.Buffer.Flags = mDesc.Uav_Flags;
			uavDesc.Buffer.NumElements = mDesc.Num_Elements;
			uavDesc.Format = mDesc.Format;
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			if (FAILED(Device->CreateUnorderedAccessView(mBuf, &uavDesc, &mUav))) return false;
		}

		if (mDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
			ZeroMemory(&srvDesc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
			srvDesc.Format = mDesc.Format;
			if (mDesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS) {
				srvDesc.BufferEx.FirstElement = 0;
				srvDesc.BufferEx.NumElements = mDesc.Num_Elements;
				srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
			}
			else
			{
				srvDesc.Buffer.FirstElement = 0;
				srvDesc.Buffer.NumElements = mDesc.Num_Elements;
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			}
			
			if (FAILED(Device->CreateShaderResourceView(mBuf, &srvDesc, &mSrv)))
				return false;
		}

		// create the staging buffer
		if (mDesc.CPUAccessFlag & D3D11_CPU_ACCESS_READ)
		{
			bufDesc.BindFlags = 0;
			bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			bufDesc.Usage = D3D11_USAGE_STAGING;
			if (FAILED(Device->CreateBuffer(&bufDesc, NULL, &mStaging))) return false;
		}
		return true;
	}

	void BufferD3D11::D3DRelease()
	{
		SAFE_RELEASE(mBuf);
		SAFE_RELEASE(mUav);
		SAFE_RELEASE(mSrv);
		SAFE_RELEASE(mStaging);
	}

	void BufferD3D11::CopyToCpu(ID3D11DeviceContext* immediateContext)
	{
		if ((mDesc.CPUAccessFlag & D3D11_CPU_ACCESS_READ) == 0) return;

		immediateContext->CopyResource(mStaging, mBuf);

		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(immediateContext->Map(mStaging, 0, D3D11_MAP_READ, 0, &mapped)))
		{
			BYTE* mappedData = reinterpret_cast<BYTE*>(mapped.pData);	// destination
			BYTE* buffer = (BYTE*)&(mData[0]);							// source
			int rowspan = mDesc.Num_Elements * mDesc.Size_Element;		// row-pitch in source
			memcpy(
				buffer,
				mappedData,
				rowspan);
			immediateContext->Unmap(mStaging, 0);
		}
	}
}