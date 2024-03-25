#pragma once

#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include "rendertarget2d.hpp"
#include <Eigen/eigen>

namespace fs = std::filesystem;

class Colormap
{
public:
	Colormap(std::string path);
	~Colormap();

	std::vector<Eigen::Vector4f> LoadColorMap(std::string path);
	bool CreateTexture(ID3D11Device* Device, std::string path);
	void ReleaseTexture();

	vislab::RenderTarget2dD3D11& GetColormapTexture() { return mColormapTexture; };
	std::vector<std::string> GetColormapList() { return mColormapList; };
	std::string GetCurrentColormap() { return mCurrentColormap; };

private:
	std::string mCurrentColormap;
	std::vector<std::string> mColormapList;
	vislab::RenderTarget2dD3D11 mColormapTexture;
};
