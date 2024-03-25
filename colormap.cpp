#include "colormap.hpp"


Colormap::Colormap(std::string path)
{
    for (const auto& entry : fs::directory_iterator(path))
	{
		mColormapList.push_back(entry.path().string());
	}
}

Colormap::~Colormap()
{
	ReleaseTexture();
}


bool Colormap::CreateTexture(ID3D11Device* Device, std::string path)
{
	std::vector<Eigen::Vector4f> values = Colormap::LoadColorMap(path);
	if (values.empty())
	{
		return false;
	}

	vislab::RenderTarget2dD3D11::TargetDesc desc;
	desc.Resolution = Eigen::Vector2i(values.size(), 1);
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.AccessFlag = D3D11_CPU_ACCESS_READ;
	mColormapTexture = vislab::RenderTarget2dD3D11(desc);

	D3D11_SUBRESOURCE_DATA init;
	ZeroMemory(&init, sizeof(D3D11_SUBRESOURCE_DATA));
	init.pSysMem = values.data();
	init.SysMemPitch = sizeof(Eigen::Vector3f) * values.size();
	init.SysMemSlicePitch = sizeof(Eigen::Vector3f) * values.size() * 1;
	if (!mColormapTexture.D3DCreate(Device, &init))
		return false;

	mCurrentColormap = path;
}

void Colormap::ReleaseTexture()
{
	mColormapTexture.D3DRelease();
}

std::vector<Eigen::Vector4f> Colormap::LoadColorMap(std::string path)
{
	std::ifstream myfile(path);
	if (!myfile.is_open()) 
	{
		return {};
	}

	std::vector<Eigen::Vector4f> colormap;
	std::string line;
	while (myfile.good())
	{
		std::getline(myfile, line);
		Eigen::Vector4f row;
		if (sscanf_s(line.c_str(), "%f,%f,%f", &row[0], &row[1], &row[2]) == 3)
		{
			row[3] = 1.f;
			colormap.push_back(row);
		}
	}
	myfile.close();
	return colormap;
}