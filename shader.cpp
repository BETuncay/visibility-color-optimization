#include "shader.hpp"


bool vislab::ShaderD3D11::D3DCreate(ID3D11Device* Device, std::string ShaderName, unsigned ShaderFlags)
{
	if (ShaderFlags & vislab::VS_FLAG)
	{
		std::string name = ShaderName + ".vso";
		if (!D3D::LoadVertexShaderFromFile(name.c_str(), Device, &mVs, &blob, &size))
			return false;
	}
	
	if (ShaderFlags & vislab::GS_FLAG)
	{
		std::string name = ShaderName + ".gso";
		if (!D3D::LoadGeometryShaderFromFile(name.c_str(), Device, &mGs))
			return false;
	}

	if (ShaderFlags & vislab::PS_FLAG)
	{
		std::string name = ShaderName + ".pso";
		if (!D3D::LoadPixelShaderFromFile(name.c_str(), Device, &mPs))
			return false;
	}

	if (ShaderFlags & vislab::CS_FLAG)
	{
		std::string name = ShaderName + ".cso";
		if (!D3D::LoadComputeShaderFromFile(name.c_str(), Device, &mCs))
			return false;
	}
	return true;
}


void vislab::ShaderD3D11::D3DRelease()
{
	FreeBlob();

	if (mVs) 
		mVs->Release();
	if (mGs)
		mGs->Release();
	if (mPs)
		mPs->Release();
	if (mCs)
		mCs->Release();
	
	mVs = NULL;
	mGs = NULL;
	mPs = NULL;
	mCs = NULL;
}

void vislab::ShaderD3D11::FreeBlob()
{
	if (blob)
		delete[] blob;
	blob = NULL;
}

void vislab::ShaderD3D11::SetShader(ID3D11DeviceContext* ImmediateContext)
{
	ImmediateContext->VSSetShader(mVs, NULL, 0);
	ImmediateContext->GSSetShader(mGs, NULL, 0);
	ImmediateContext->PSSetShader(mPs, NULL, 0);
	//ImmediateContext->PSSetShader(mPs, NULL, 0);
}
