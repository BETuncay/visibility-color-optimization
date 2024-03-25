#pragma once

#include <d3d11.h>
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx11.h>
#include "renderer.hpp"
#include "scene.hpp"
#include <iostream>


class ImguiHelper
{
public:
	ImguiHelper(HWND& hWnd);
	~ImguiHelper();

	void Create(HWND& hWnd, ID3D11Device* device, ID3D11DeviceContext* immediateContext);
	void Draw(ID3D11DeviceContext* immediateContext, ID3D11RenderTargetView* renderTarget, Renderer* g_Renderer, D3D* g_D3D, Colormap* g_Colormap, Camera* g_Camera, Lines* Geometry, Scene* g_Scene);
	void HistogramEqualizationWindow(ID3D11DeviceContext* immediateContext, Renderer* g_Renderer, D3D* g_D3D, Colormap* g_Colormap);
	void HistogramDampingWindow(ID3D11DeviceContext* immediateContext, Renderer* g_Renderer, D3D* g_D3D);
	void ColormapWindow(ID3D11DeviceContext* immediateContext, Renderer* g_Renderer, D3D* g_D3D, Colormap* g_Colormap);
	void CameraWindow(Camera* g_Camera);
	void RenderParameterWindow(ID3D11DeviceContext* immediateContext, Renderer* g_Renderer);
	void SceneWindow(ID3D11DeviceContext* immediateContext, D3D* g_D3D, Lines* Geometry, Scene* g_Scene);

	void PlotHistogram(ID3D11DeviceContext* immediateContext, vislab::RenderTarget2dD3D11& Histogram, const char* text);
	void PlotCDF(ID3D11DeviceContext* immediateContext, vislab::BufferD3D11& CDF, const char* text);
	void PlotHistogramAndCDF(ID3D11DeviceContext* immediateContext, vislab::RenderTarget2dD3D11& Histogram, vislab::BufferD3D11& CDF, const char* text1, const char* text2);
	bool CheckConfig(ImGuiConfigFlags_ flag);

private:
	ImGuiIO* io;
	bool show_demo_window;
	std::vector<std::string> mDatasets;
	std::string mCurrentDataset;
	XMFLOAT3 mStoreForward1, mStoreForward2;
};