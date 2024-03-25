#include "math.hpp"
#include "d3d.hpp"
#include "cbuffer.hpp"
#include "camera.hpp"
#include "lines.hpp"
#include "renderer.hpp"
#include "imgui_helper.hpp"
#include "colormap.hpp"
#include "scene.hpp"
#include <Windows.h>
#include <windowsx.h>
#include "gpuprofiler.hpp"


#define ENABLE_IMGUI // uncomment this line to enable the gui

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

D3D* g_D3D = NULL;
Camera* g_Camera = NULL;
Lines* g_Lines = NULL;
Renderer* g_Renderer = NULL;
ImguiHelper* g_Imgui = NULL;
Colormap* g_Colormap = NULL;
Scene* g_Scene = NULL;
static UINT g_ResizeWidth = 0, g_ResizeHeight = 0;


//// ---------------------------------------
//// Window-related code
//// ---------------------------------------
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
			return 0;
		g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
		g_ResizeHeight = (UINT)HIWORD(lParam);
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_DPICHANGED:
		if (g_Imgui->CheckConfig(ImGuiConfigFlags_DpiEnableScaleViewports))
		{
			const RECT* suggested_rect = (RECT*)lParam;
			SetWindowPos(hWnd, nullptr, suggested_rect->left, suggested_rect->top, suggested_rect->right - suggested_rect->left, suggested_rect->bottom - suggested_rect->top, SWP_NOZORDER | SWP_NOACTIVATE);
		}
		break;
	}
	return DefWindowProcA(hWnd, msg, wParam, lParam);
}

HRESULT InitWindow(HINSTANCE hInstance, int nCmdShow, const std::string& WindowTitle, LONG Width, LONG Height, HWND& hWnd)
{
	// Register class
	WNDCLASSEX wcex;
	ZeroMemory(&wcex, sizeof(WNDCLASSEX));
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc; // DefWindowProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = "SmallDOOClass";
	if (!RegisterClassEx(&wcex))
		return E_FAIL;

	// Create window
	RECT rc = { 0, 0, Width, Height };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
	hWnd = CreateWindow("SmallDOOClass", 
		WindowTitle.c_str(), 
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 
		CW_USEDEFAULT, 
		rc.right - rc.left, 
		rc.bottom - rc.top, 
		NULL, 
		NULL, 
		hInstance,
		NULL
	);
	if (!hWnd)
		return E_FAIL;

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);
	return S_OK;
}

int probe = 0;
// ---------------------------------------
// Update and Render
// ---------------------------------------
void Render(double dt)
{
	// Collect objects needed later
	ID3D11DeviceContext* immediateContext = g_D3D->GetImmediateContext();
	ID3D11RenderTargetView* rtvBackbuffer = g_D3D->GetRtvBackbuffer();
	ID3D11DepthStencilView* dsvBackbuffer = g_D3D->GetDsvBackbuffer();

	g_gpuProfiler.BeginFrame(immediateContext);

	// Clear the backbuffer and the depth buffer
	float clearColor[4] = { 1,1,1,1 };
	immediateContext->ClearRenderTargetView(rtvBackbuffer, clearColor);
	immediateContext->ClearDepthStencilView(dsvBackbuffer, D3D11_CLEAR_DEPTH, 1, 0);

	// Render the scene with decoupled opacity optimization
	g_Renderer->Draw(immediateContext, g_D3D, g_Lines, g_Camera, g_Colormap, dt);

	#ifdef ENABLE_IMGUI
		g_Imgui->Draw(immediateContext, rtvBackbuffer, g_Renderer, g_D3D, g_Colormap, g_Camera, g_Lines, g_Scene);
	#endif

	g_gpuProfiler.WaitForDataAndUpdate(immediateContext);

	float dTDrawTotal = 0.0f;
	for (GTS gts = GTS_BeginFrame; gts < GTS_EndFrame; gts = GTS(gts + 1))
		dTDrawTotal += g_gpuProfiler.DtAvg(gts);

	if (probe > 16)
	{
		printf(
			"Draw time: %0.3f ms\n"
			"   Create List Low: %0.3f ms\n"
			"   Sort List Low: %0.3f ms\n"
			"   Gather Alpha: %0.3f ms\n"
			"   Smoothing: %0.3f ms\n"
			"   Fade Alpha: %0.3f ms\n"
			"   Create List: %0.3f ms\n"
			"   Sort List: %0.3f ms\n"
			"   Histogram: %0.3f ms\n"
			"   Histogram CDF: %0.3f ms\n"
			"   Exponential Damping: %0.3f ms\n"
			"   Render image: %0.3f ms\n"
			"GPU frame time: %0.3f ms\n",
			1000.0f * dTDrawTotal,
			1000.0f * g_gpuProfiler.DtAvg(GTS_CreateListLow),
			1000.0f * g_gpuProfiler.DtAvg(GTS_SortListLow),
			1000.0f * g_gpuProfiler.DtAvg(GTS_GatherAlpha),
			1000.0f * g_gpuProfiler.DtAvg(GTS_Smoothing),
			1000.0f * g_gpuProfiler.DtAvg(GTS_FadeAlpha),
			1000.0f * g_gpuProfiler.DtAvg(GTS_CreateList),
			1000.0f * g_gpuProfiler.DtAvg(GTS_SortList),
			1000.0f * g_gpuProfiler.DtAvg(GTS_Histogram),
			1000.0f * g_gpuProfiler.DtAvg(GTS_HistogramCDF),
			1000.0f * g_gpuProfiler.DtAvg(GTS_ExponentialDamping),
			1000.0f * g_gpuProfiler.DtAvg(GTS_RenderImage),
			1000.0f * (dTDrawTotal + g_gpuProfiler.DtAvg(GTS_EndFrame)));
		probe = 0;
	}
	else
		probe++;
	
	// Display frame on-screen and finish up queries

	// Swap
	g_D3D->GetSwapChain()->Present(0, 0);
	g_gpuProfiler.EndFrame(immediateContext);
}

// =============================================================
// =============================================================
// =============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Allocate a debug console and redirect standard streams
	AllocConsole();
	FILE* pCout, * pCerr;
	freopen_s(&pCout, "CONOUT$", "w", stdout);
	freopen_s(&pCerr, "CONOUT$", "w", stderr);

	printf("Decoupled Opacity Optimization Demo\n");
	printf("===================================\n\n");
	printf("Move the camera by holding the right mouse button and move back and forth with 'W' and 'S'\n\n");
	printf("Use '0', '1' or '2' as command line argument to select a data set:\n");
	printf("   0 = data/tornado.obj (default)\n");
	printf("   1 = data/rings.obj\n");
	printf("   2 = data/heli.obj\n");

	// =============================================================
	// initialize
	// =============================================================
	int datasetIndex = 0;
	//datasetIndex = atoi(lpCmdLine);		// read 0, 1 or 2 from command line

	/// amira
	/// https://gitlab.cs.fau.de/i9vc/teaching/ss23/vispro-ss23/-/tree/main/common?ref_type=heads

	std::string path, distanceMatrixPath, colormap_folder_path, colormap_path, dataset;
	colormap_folder_path = "CETperceptual_csv_0_1";
	colormap_path = "CETperceptual_csv_0_1\\CET-C1.csv";
	Vec3f eye, lookAt;
	Vec2i resolution(1000, 1000);
	float q, r, lambda, stripWidth;
	int totalNumCPs, smoothingIterations, clusterCount;
	clusterCount = 0;
	RepresentativeMethod representativeMethod = RepresentativeMethod::MostImportantCurve;
	Renderer::HistogramMode histogramMode = Renderer::HistogramMode::Equalization;
	std::vector<float> segments = { 0.f, 0.5f, 1.f };	// all elements \in [0, 1] AND first, last element MUST be 0, 1 
	switch (datasetIndex)
	{
	default:
	case 0:
		path = std::string("data\\tornado.obj");
		eye = Vec3f(11, 21, -25);
		lookAt = Vec3f(10, 10, 10);
		q = 60;
		r = 500;
		lambda = 1;
		stripWidth = 0.05f;
		totalNumCPs = 15000;
		smoothingIterations = 100;
		break;
	case 1:
		path = std::string("data\\rings.obj");
		eye = Vec3f(40, 16, 12);
		lookAt = Vec3f(10, 10, 10);
		q = 0;// 80;
		r = 0;//40;
		lambda = 1;//1.5f;
		stripWidth = 0.03f;
		totalNumCPs = 15000;
		smoothingIterations = 10;
		break;
	case 2:
		path = std::string("data\\heli.obj");
		eye = Vec3f(40, 4, 17);
		lookAt = Vec3f(10, 10, 10);
		q = 100;
		r = 200;
		lambda = 2;
		stripWidth = 0.03f;
		totalNumCPs = 15000;
		smoothingIterations = 10;
		break;
	case 3:
		path = std::string("data\\borromean---lineLength---vorticity.obj");
		distanceMatrixPath = std::string("distanceMatrix\\borromean---rMCPD---0.000000---0.000000---Normalized.dist");
		colormap_path = "CETperceptual_csv_0_1\\CET-L08.csv";
		clusterCount = 50;
		representativeMethod = RepresentativeMethod::MostImportantCurve;
		eye = Vec3f(2.06363845, 5.65614271, -8.71838760);
		lookAt = Vec3f(-0.240078673, -0.566406906, 0.788381577) + eye;
		q = 90;
		r = 120;
		lambda = 1.1;
		stripWidth = 0.05f;
		totalNumCPs = 15000;
		smoothingIterations = 10;
		break;
	case 4:
		path = std::string("data\\trefoil10---lineLength---lineLength.obj");
		distanceMatrixPath = std::string("distanceMatrix\\trefoil10---rMCPD---0.000000---0.000000---Normalized.dist");
		colormap_path = "CETperceptual_csv_0_1\\CET-L06.csv";
		clusterCount = 120;
		representativeMethod = RepresentativeMethod::MostImportantCurve;
		eye = Vec3f(1.41283929, 3.87762308, -6.32018995);
		lookAt = Vec3f(-0.171248317, -0.516534030, 0.838967621) + eye;
		q = 20;
		r = 20;
		lambda = 1;
		stripWidth = 0.05f;
		totalNumCPs = 15000;
		smoothingIterations = 10;
		break;
	case 5:
		path = std::string("data\\benzene---lineLength---linelength.obj");
		distanceMatrixPath = std::string("distanceMatrix\\benzene---rMCPD---0.000000---0.000000---Normalized.dist");
		colormap_path = "CETperceptual_csv_0_1\\CET-L06.csv";
		clusterCount = 1000;
		representativeMethod = RepresentativeMethod::MostImportantCurve;
		eye = Vec3f(45.8729324, 58.5419846, -1.43217289); 
		lookAt = Vec3f(0.0768424869, -0.158158809, 0.984419167) + eye;
		q = 80;
		r = 80;
		lambda = 1;
		stripWidth = 0.07f;
		totalNumCPs = 15000;
		smoothingIterations = 10;
		dataset = "benzene";
		break;
	case 6:
		path = std::string("data\\borromean---lineLength---lineLength.obj");
		distanceMatrixPath = std::string("distanceMatrix\\borromean---rMCPD---0.000000---0.000000---Normalized.dist");
		colormap_path = "CETperceptual_csv_0_1\\CET-L06.csv";
		clusterCount = 1000;
		representativeMethod = RepresentativeMethod::MostImportantCurve;
		eye = Vec3f(2.06363845, 5.65614271, -8.71838760);
		lookAt = Vec3f(-0.240078673, -0.566406906, 0.788381577) + eye;
		q = 80;
		r = 80;
		lambda = 1;
		stripWidth = 0.03f;
		totalNumCPs = 15000;
		smoothingIterations = 10;
		dataset = "borromean";
		break;
	case 7:
		path = std::string("data\\delta65_high---lineLength---lineLength.obj");
		distanceMatrixPath = std::string("distanceMatrix\\delta65_high---rMCPD---0.000000---0.000000---Normalized.dist");
		colormap_path = "CETperceptual_csv_0_1\\CET-L06.csv";
		clusterCount = 1000;
		representativeMethod = RepresentativeMethod::MostImportantCurve;
		eye = Vec3f(2.06363845, 5.65614271, -8.71838760);
		lookAt = Vec3f(-0.240078673, -0.566406906, 0.788381577) + eye;
		q = 80;
		r = 80;
		lambda = 1;
		stripWidth = 0.05f;
		totalNumCPs = 15000;
		smoothingIterations = 10;
		dataset = "delta65_high";
		break;
	case 8:
		path = std::string("data\\ECMWF_3D_Reanalysis_Velocity_T0---lineLength---lineLength.obj");
		distanceMatrixPath = std::string("distanceMatrix\\ECMWF_3D_Reanalysis_Velocity_T0---rMCPD---0.000000---0.000000---Normalized.dist");
		colormap_path = "CETperceptual_csv_0_1\\CET-L06.csv";
		clusterCount = 1000;
		representativeMethod = RepresentativeMethod::MostImportantCurve;
		eye = Vec3f(789.495728, 233.608536, -473.939209);
		lookAt = Vec3f(0.0785621554, -0.158158809, 0.984283388) + eye;
		q = 80;
		r = 80;
		lambda = 1;
		stripWidth = 1.5f;
		totalNumCPs = 15000;
		smoothingIterations = 10;
		dataset = "ECMWF_3D_Reanalysis_Velocity_T0";
		break;
	case 9:
		path = std::string("data\\trefoil10---lineLength---lineLength.obj");
		distanceMatrixPath = std::string("distanceMatrix\\trefoil10---rMCPD---0.000000---0.000000---Normalized.dist");
		colormap_path = "CETperceptual_csv_0_1\\CET-L06.csv";
		clusterCount = 1000;
		representativeMethod = RepresentativeMethod::MostImportantCurve;
		eye = Vec3f(2.06363845, 5.65614271, -8.71838760);
		lookAt = Vec3f(-0.40078673, -0.566406906, 0.788381577) + eye;
		q = 80;
		r = 80;
		lambda = 1;
		stripWidth = 0.03f;
		totalNumCPs = 15000;
		smoothingIterations = 10;
		dataset = "trefoil10";
		break;
	case 10:
		path = std::string("data\\UCLA_CTBL_Velocity_T6---lineLength---lineLength.obj");
		distanceMatrixPath = std::string("distanceMatrix\\UCLA_CTBL_Velocity_T6---rMCPD---0.000000---0.000000---Normalized.dist");
		colormap_path = "CETperceptual_csv_0_1\\CET-L06.csv";
		clusterCount = 600;
		representativeMethod = RepresentativeMethod::MostImportantCurve;
		eye = Vec3f(204.534515, 214.596008, -450.818115);
		lookAt = Vec3f(-0.0163300019, -0.0436201319, 0.998914719) + eye;
		q = 80;
		r = 80;
		lambda = 1;
		stripWidth = 0.5f;
		totalNumCPs = 15000;
		smoothingIterations = 10;
		dataset = "UCLA_CTBL_Velocity_T6";
		break;
	}
	printf(("Currently using: " + path + "\n\n").c_str());

	// Create the window
	HWND hWnd;
	if (FAILED(InitWindow(hInstance, nCmdShow, "Decoupled Opacity Optimization Demo", resolution.x, resolution.y, hWnd)))
		return -1;

	// Initialize the objects
	g_D3D = new D3D(hWnd);
	ID3D11Device* device = g_D3D->GetDevice();
	ID3D11DeviceContext* immediateContext = g_D3D->GetImmediateContext();
	ID3D11RenderTargetView* rtvBackbuffer = g_D3D->GetRtvBackbuffer();

	g_Camera = new Camera(eye, lookAt, (float)resolution.x / (float)resolution.y, hWnd);
	g_Lines = new Lines(path, distanceMatrixPath, representativeMethod, totalNumCPs, clusterCount, device);
	g_Renderer = new Renderer(q, r, lambda, stripWidth, smoothingIterations, histogramMode, segments, device, & g_D3D->GetBackBufferSurfaceDesc());
	g_Imgui = new ImguiHelper(hWnd);
	g_Colormap = new Colormap(colormap_folder_path);
	g_Scene = new Scene("data", "distanceMatrix", "---", 0);

	// Create D3D resources
	g_Camera->Create(device);
	g_Imgui->Create(hWnd, device, immediateContext);
	g_Renderer->InitBufferData(immediateContext);
	g_Colormap->CreateTexture(device, colormap_path); 	// create colormap and load default colorschema

	g_gpuProfiler.Init(device);							// profiler code

	// ---------------------------------------
	// Enter the main loop
	// ---------------------------------------
	LARGE_INTEGER timerLast, timerCurrent, frequency;
	QueryPerformanceCounter(&timerCurrent);
	timerLast = timerCurrent;

	MSG msg = { 0 };
	while (WM_QUIT != msg.message)
	{
		// handle windows messages
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		// close window on 'ESC'
		if (GetAsyncKeyState(VK_ESCAPE) != 0) break;

		// get elapsed time
		QueryPerformanceCounter(&timerCurrent);
		QueryPerformanceFrequency(&frequency);
		LONGLONG timeElapsed = timerCurrent.QuadPart - timerLast.QuadPart;
		double elapsedS = (double)timeElapsed / (double)frequency.QuadPart;
		timerLast = timerCurrent;

		// update the camera
		g_Camera->Update((float) elapsedS);
		// render the scene
		Render(elapsedS);
		//printf("\rfps: %i      ", (int)(1.0 / elapsedS));
	}

	// read histogram values
	vislab::RenderTarget2dD3D11& HistogramTarget = g_Renderer->GetHistogram();
	vislab::RenderTarget2dD3D11& HistogramTargetNormalized = g_Renderer->GetHistogramNormalized();
	HistogramTarget.CopyToCpu(immediateContext);
	HistogramTargetNormalized.CopyToCpu(immediateContext);

	{
		float* histogram = &HistogramTarget.Data[0];
		std::string hist = "histogram_" + dataset + std::to_string(HISTOGRAM_BINS) + ".txt";
		std::ofstream myfile(hist);
		if (myfile.is_open())
		{
			for (int i = 0; i < HISTOGRAM_BINS; i++) 
			{
				myfile << histogram[i] << " ";
			}
			myfile.close();
		}
	}
	 
	{
		float* histogram_norm = &HistogramTargetNormalized.Data[0];
		std::string hist = "histogramNorm" + std::to_string(HISTOGRAM_BINS) + ".txt";
		std::ofstream myfile(hist);
		if (myfile.is_open())
		{
			for (int i = 0; i < HISTOGRAM_BINS; i++)
			{
				myfile << histogram_norm[i] << " ";
			}
			myfile.close();
		}
	}
	
	// Clean up before closing.
	delete g_Camera;
	delete g_Lines;
	delete g_Renderer;
	delete g_D3D;
	delete g_Imgui;
	delete g_Colormap;

	g_gpuProfiler.Shutdown();
	return 0;
}