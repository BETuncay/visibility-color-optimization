#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <d3d11_1.h>
#include <Eigen/eigen>

namespace vislab
{
	// Defines all properties of the D3D resource.
	struct BufferDesc
	{
		unsigned Num_Elements;
		unsigned Size_Element;
		unsigned BindFlags;
		unsigned CPUAccessFlag;
		unsigned MiscFlags;
		DXGI_FORMAT Format = DXGI_FORMAT_UNKNOWN;
		unsigned Uav_Flags = 0;
		BufferDesc() : Num_Elements(0), Size_Element(0), BindFlags(0), CPUAccessFlag(0), MiscFlags(0), Format(DXGI_FORMAT_UNKNOWN), Uav_Flags(0)
		{}
		BufferDesc(unsigned Num_Elements, unsigned Size_Element, unsigned BindFlags, unsigned CPUAccessFlag, unsigned MiscFlags, DXGI_FORMAT Format, unsigned Uav_Flags) :
			Num_Elements(Num_Elements),
			Size_Element(Size_Element),
			BindFlags(BindFlags),
			CPUAccessFlag(CPUAccessFlag),
			MiscFlags(MiscFlags),
			Format(Format),
			Uav_Flags(Uav_Flags)
		{}
	};

	// A class that manages buffers.
	class BufferD3D11
	{
	public:
	

		BufferD3D11(const BufferDesc& desc = BufferDesc());
		BufferD3D11(const BufferD3D11& other);
		~BufferD3D11();

		bool D3DCreate(ID3D11Device* Device);
		void D3DRelease();
		void CopyToCpu(ID3D11DeviceContext* immediateContext);

		ID3D11Buffer* GetBuf() { return mBuf; }
		ID3D11UnorderedAccessView* GetUav() { return mUav; }
		ID3D11ShaderResourceView* GetSrv() { return mSrv; }

		std::vector<byte> mData;
		BufferDesc mDesc;
	private:
		ID3D11Buffer* mBuf;
		ID3D11UnorderedAccessView* mUav;
		ID3D11ShaderResourceView* mSrv;
		ID3D11Buffer* mStaging;
	};
}