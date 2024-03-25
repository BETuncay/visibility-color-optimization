#pragma once

#include <d3d11.h>
#include <dxgi.h>
#include <d3d11_1.h>
#include <string>
#include "d3d.hpp"



namespace vislab
{

	enum ShaderFlag {
		VS_FLAG = 0x1,
		GS_FLAG = 0x2,
		PS_FLAG = 0x4,
		CS_FLAG = 0x8,
	};

	// A class that manages buffers.
	class ShaderD3D11
	{
	public:
	
		ShaderD3D11() : 
			mVs(NULL),
			mGs(NULL),
			mPs(NULL),
			mCs(NULL),
			blob(NULL),
			size(0)
		{}
		ShaderD3D11::~ShaderD3D11()
		{
			D3DRelease();
		}

		bool D3DCreate(ID3D11Device* Device, std::string ShaderName, unsigned ShaderFlags);
		void D3DRelease();
		void SetShader(ID3D11DeviceContext* ImmediateContext);
		void FreeBlob();

		char* GetBlob() { return blob; }
		UINT GetSize() { return size; }
		ID3D11VertexShader* GetVs() { return mVs; }
		ID3D11GeometryShader* GetGs() { return mGs; }
		ID3D11PixelShader* GetPs() { return mPs; }
		ID3D11ComputeShader* GetCs() { return mCs; }

	private:

		ID3D11VertexShader* mVs;
		ID3D11GeometryShader* mGs;
		ID3D11PixelShader* mPs;
		ID3D11ComputeShader* mCs;

		char* blob;
		UINT size;
	};
}