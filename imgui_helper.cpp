#include "imgui_helper.hpp"

ImguiHelper::ImguiHelper(HWND& hWnd) :
	show_demo_window(true),
	io(NULL),
	mDatasets({})
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	io = &ImGui::GetIO();
	io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io->ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
	io->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows

	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	if (io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	// load all datasets
	std::string path = "data";
	for (const auto& entry : fs::directory_iterator(path))
	{
		mDatasets.push_back(entry.path().string());
	}
}

ImguiHelper::~ImguiHelper()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void ImguiHelper::Create(HWND& hWnd, ID3D11Device* device, ID3D11DeviceContext* immediateContext)
{
	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(device, immediateContext);
}

void ImguiHelper::Draw(ID3D11DeviceContext* immediateContext, ID3D11RenderTargetView* renderTarget, Renderer* g_Renderer, D3D* g_D3D, Colormap* g_Colormap, Camera* g_Camera, Lines* Geometry, Scene* g_Scene)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	if (show_demo_window)
	{
		ImGui::ShowDemoWindow(&show_demo_window);
	}

	SceneWindow(immediateContext, g_D3D, Geometry, g_Scene);
	CameraWindow(g_Camera);
	RenderParameterWindow(immediateContext, g_Renderer);
	ColormapWindow(immediateContext, g_Renderer, g_D3D, g_Colormap);
	HistogramDampingWindow(immediateContext, g_Renderer, g_D3D);
	HistogramEqualizationWindow(immediateContext, g_Renderer, g_D3D, g_Colormap);


	ImGui::Render();
	immediateContext->OMSetRenderTargets(1, &renderTarget, nullptr);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	if (io->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void ImguiHelper::HistogramEqualizationWindow(ID3D11DeviceContext* immediateContext, Renderer* g_Renderer, D3D* g_D3D, Colormap* g_Colormap)
{
	ImGui::Begin("Histogram Equalization");

	int histogramMode = (int) g_Renderer->GetHistogramMode();
	ImGui::RadioButton("None", &histogramMode, 0); ImGui::SameLine();
	ImGui::RadioButton("Histogram Equalization", &histogramMode, 1); ImGui::SameLine();
	ImGui::RadioButton("Histogram Bi Equalization", &histogramMode, 2); ImGui::SameLine();
	ImGui::RadioButton("Scalar Color", &histogramMode, 3);
	ImGui::RadioButton("Segmented HE", &histogramMode, 4);

	if (histogramMode != (int)g_Renderer->GetHistogramMode())
	{
		g_Renderer->SetHistogramMode((Renderer::HistogramMode)histogramMode);
	}

	if ((Renderer::HistogramMode)histogramMode != Renderer::HistogramMode::None)
	{
		g_Renderer->CalculateNormalizedHistogram(immediateContext, g_D3D);
	}

	// plot histograms
	{
		vislab::RenderTarget2dD3D11& HistogramTarget = g_Renderer->GetHistogram();
		vislab::RenderTarget2dD3D11& HistogramTargetNormalized = g_Renderer->GetHistogramNormalized();
		HistogramTarget.CopyToCpu(immediateContext);		
		HistogramTargetNormalized.CopyToCpu(immediateContext);

		PlotHistogram(immediateContext, HistogramTarget, "Histogram");
		ImGui::SameLine();
		PlotHistogram(immediateContext, HistogramTargetNormalized, "Normalized Histogram");
	}
	
	// plot colorbars
	{
		ImGui::Image(g_Colormap->GetColormapTexture().GetSrv(), ImVec2(300, 50.0f));
		ImGui::SameLine();
		ImGui::Image(g_Colormap->GetColormapTexture().GetSrv(), ImVec2(300, 50.0f));
	}

	// plot cdf
	//if (show_histogram_normalization)
	{
		vislab::BufferD3D11& HistogramCDF = g_Renderer->GetHistogramCDF();
		vislab::BufferD3D11& HistogramCDFNormalized = g_Renderer->GetHistogramCDFNormalized();
		HistogramCDF.CopyToCpu(immediateContext);
		HistogramCDFNormalized.CopyToCpu(immediateContext);
		
		PlotCDF(immediateContext, HistogramCDF, "CDF");
		ImGui::SameLine();
		PlotCDF(immediateContext, HistogramCDFNormalized, "Normalized CDF");
	}

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io->Framerate, io->Framerate);
	ImGui::End();
}

void ImguiHelper::HistogramDampingWindow(ID3D11DeviceContext* immediateContext, Renderer* g_Renderer, D3D* g_D3D)
{
	ImGui::Begin("Histogram Damping");

	vislab::BufferD3D11& HistogramCDF = g_Renderer->GetHistogramCDF();
	vislab::BufferD3D11& HistogramCDFDamped = g_Renderer->GetHistogramCDFExponentialDamping();

	HistogramCDF.CopyToCpu(immediateContext);
	HistogramCDFDamped.CopyToCpu(immediateContext);

	float halflife = g_Renderer->GetDampingHalflife();
	ImGui::SliderFloat("Halflife", &halflife, 0.f, 1.f);
	g_Renderer->SetDampingHalflife(halflife);
	{
		float* cdf = (float*)&HistogramCDF.mData[0];
		ImGui::PlotLines("", cdf, HISTOGRAM_BINS, 0.f, "CDF", 0.f, 1.0f, ImVec2(300, 300.0f));
	}
	ImGui::SameLine();
	{
		float* cdf = (float*)&HistogramCDFDamped.mData[0];
		ImGui::PlotLines("", cdf, HISTOGRAM_BINS, 0.f, "Damped CDF", 0.f, 1.0f, ImVec2(300, 300.0f));
	}
	
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io->Framerate, io->Framerate);
	ImGui::End();
}

void ImguiHelper::ColormapWindow(ID3D11DeviceContext* immediateContext, Renderer* g_Renderer, D3D* g_D3D, Colormap* g_Colormap)
{
	ImGui::Begin("Colormap");

	std::vector<std::string> items = g_Colormap->GetColormapList();
	std::string currentItem = g_Colormap->GetCurrentColormap();

	const char* current_item = currentItem.c_str();
	if (ImGui::BeginCombo("##combo", current_item)) // The second parameter is the label previewed before opening the combo.
	{
		for (int n = 0; n < items.size(); n++)
		{
			const char* item_n = items[n].c_str();
			bool is_selected = (current_item == item_n); // You can store your selection however you want, outside or inside your objects
			if (ImGui::Selectable(item_n, is_selected))
				current_item = item_n;
				if (is_selected)
					ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
		}
		ImGui::EndCombo();
	}

	std::string path(current_item);
	if (path != currentItem)
	{
		g_Colormap->ReleaseTexture();
		g_Colormap->CreateTexture(g_D3D->GetDevice(), path);
	}

	ImGui::Image(g_Colormap->GetColormapTexture().GetSrv(), ImVec2(300, 100.0f));

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io->Framerate, io->Framerate);
	ImGui::End();
}

void ImguiHelper::CameraWindow(Camera* g_Camera)
{
	ImGui::Begin("Camera");

	float speed = g_Camera->GetCameraSpeed();
	ImGui::SliderFloat("Speed", &speed, 0.f, 50.f);
	g_Camera->SetCameraSpeed(speed);

	float fov = g_Camera->GetFov();
	ImGui::SliderAngle("Fov", &fov, 1.f, 180.f);
	g_Camera->SetFov(fov);

	//float nearValue = g_Camera->GetNear();
	//ImGui::SliderFloat("Near", &nearValue, 0.0001f, 1.f);
	//g_Camera->SetNear(nearValue);

	float farValue = g_Camera->GetFar();
	ImGui::SliderFloat("Far", &farValue, 1.f, 1000.f);
	g_Camera->SetFar(farValue);

	
	if (ImGui::Button("Store Forward 1", ImVec2(200, 50)))
	{
		mStoreForward1 = g_Camera->GetForward();
	}


	if (ImGui::Button("Store Forward 2", ImVec2(200, 50)))
	{
		mStoreForward2 = g_Camera->GetForward();
	}


	if (ImGui::Button("Set Forward 1", ImVec2(200, 50)))
	{
		g_Camera->SetForward(mStoreForward1);
	}

	if (ImGui::Button("Set Forward 2", ImVec2(200, 50)))
	{
		g_Camera->SetForward(mStoreForward2);
	}

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io->Framerate, io->Framerate);
	ImGui::End();
}

void ImguiHelper::RenderParameterWindow(ID3D11DeviceContext* immediateContext, Renderer* g_Renderer)
{
	ImGui::Begin("Render Parameters");

	float q = g_Renderer->_CbRenderer.Data.Q;
	ImGui::SliderFloat("Q (Penalty for fragments in the front)", &q, 0.f, 1000.f);
	g_Renderer->_CbRenderer.Data.Q = q;

	float r = g_Renderer->_CbRenderer.Data.R;
	ImGui::SliderFloat("R (Penalty for fragments in the back)", &r, 0.f, 1000.f);
	g_Renderer->_CbRenderer.Data.R = r;
	 
	float lambda = g_Renderer->_CbRenderer.Data.Lambda;
	ImGui::SliderFloat("Lambda (Weight of 1 - Importance)", &lambda, 0.f, 5.f);
	g_Renderer->_CbRenderer.Data.Lambda = lambda;

	float stripWidth = g_Renderer->_CbRenderer.Data.StripWidth;
	ImGui::SliderFloat("StripWidth", &stripWidth, 0.f, 1.f);
	g_Renderer->_CbRenderer.Data.StripWidth = stripWidth;

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io->Framerate, io->Framerate);
	ImGui::End();
}

void ImguiHelper::SceneWindow(ID3D11DeviceContext* immediateContext, D3D* g_D3D, Lines* Geometry, Scene* g_Scene)
{
	ImGui::Begin("Scene");

	// first combo -> dataset name
	DataDescription currentItem = g_Scene->GetCurrentDataset();
	DistanceMatrixDescription currentDistanceMatrix = g_Scene->GetCurrentDistanceMatrix();

	{
		std::set<std::string> names = g_Scene->GetDatasetNames();
		const char* currentName = currentItem.name.c_str();
		if (ImGui::BeginCombo("Dataset", currentName))
		{
			for (const std::string& name : names)
			{
				const char* temp = name.c_str();
				bool is_selected = (currentName == temp);
				if (ImGui::Selectable(temp, is_selected))
					currentName = temp;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		std::string name(currentName);
		if (name != currentItem.name)
		{
			g_Scene->SetCurrentDataset(g_Scene->GetFirstMatchingDatasets(name));
			g_Scene->RefreshCurrentDatasetImportance();
			g_Scene->RefreshCurrentDatasetScalarColor();
		}
	}
	
	{
		// second combo -> dataset importance
		std::set<std::string> importances = g_Scene->GetCurrentDatasetImportance();
		const char* currentImportance = currentItem.importance.c_str();
		if (ImGui::BeginCombo("Importance", currentImportance))
		{
			for (const std::string& importance : importances)
			{
				const char* temp = importance.c_str();
				bool is_selected = (currentImportance == temp);
				if (ImGui::Selectable(temp, is_selected))
					currentImportance = temp;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		std::string importance(currentImportance);
		if (importance != currentItem.importance)
		{
			g_Scene->SetCurrentDataset(g_Scene->GetFirstMatchingDatasets(currentItem.name, importance));
			g_Scene->RefreshCurrentDatasetScalarColor();
		}
	}
	
	{
		// third combo -> dataset scalarColor
		std::set<std::string> scalarColors = g_Scene->GetCurrentDatasetScalarColor();
		const char* currentScalarColor = currentItem.scalarColor.c_str();
		if (ImGui::BeginCombo("ScalarColor", currentScalarColor))
		{
			for (const std::string& scalarColor : scalarColors)
			{
				const char* temp = scalarColor.c_str();
				bool is_selected = (currentScalarColor == temp);
				if (ImGui::Selectable(temp, is_selected))
					currentScalarColor = temp;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		std::string scalarColor(currentScalarColor);
		if (scalarColor != currentItem.scalarColor)
		{
			currentItem.scalarColor = scalarColor;
			g_Scene->SetCurrentDataset(currentItem);
		}
	}

	if (ImGui::Button("Set Dataset", ImVec2(100.f, 20.f)))
	{
		std::string path = g_Scene->ConstructDatasetPath(currentItem);
		Geometry->SetCurrentDataSet(path);
		Geometry->Release();
		Geometry->ReleaseObject(Geometry->GetObjectData());
		Geometry->ReleaseObject(Geometry->GetClusteredObjectData());
		Geometry->SetClusterSize(0);
		Geometry->ParseLineData(path);
		Geometry->LoadLineSet(Geometry->GetObjectData());
		Geometry->Create(g_D3D->GetDevice());

		g_Scene->SetCurrentDistanceMatrix(g_Scene->GetFirstMatchingDistanceMatrix(currentItem.name));
		g_Scene->RefreshCurrentDistanceMatrixMetrics();
		DistanceMatrixDescription& changedDistanceMatrix = g_Scene->GetCurrentDistanceMatrix();
		if (changedDistanceMatrix.name != "Unknown") 
		{
			Geometry->CalculateClusterReport(g_Scene->ConstructDistanceMatrixPath(changedDistanceMatrix), g_Scene->GetLinkageMode());
		}
	}

	int linkageMode = g_Scene->GetLinkageMode();
	ImGui::RadioButton("Complete Linkage", &linkageMode, 0); ImGui::SameLine();
	ImGui::RadioButton("Single Linkage", &linkageMode, 1);

	if (linkageMode != g_Scene->GetLinkageMode())
	{
		g_Scene->SetLinkageMode(linkageMode);
	}

	{
		// fourth combo -> distance matrix selector
		std::vector<std::string> distanceMetrics = g_Scene->GetCurrentDistanceMetrics();
		const char* currentDistanceMetric = currentDistanceMatrix.metric.c_str();
		if (ImGui::BeginCombo("DistanceMatrix", currentDistanceMetric))
		{
			for (const std::string& distanceMatrix : distanceMetrics)
			{
				const char* temp = distanceMatrix.c_str();
				bool is_selected = (currentDistanceMetric == temp);
				if (ImGui::Selectable(temp, is_selected))
					currentDistanceMetric = temp;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		// fith combo -> representant selector
		RepresentativeMethod currentMethod = Geometry->GetRepresentativeMethod();
		std::string currentMethodString = Geometry->RepresentativeToString(currentMethod);
		if (ImGui::BeginCombo("RepresentantMethod", currentMethodString.c_str()))
		{
			for (int method = RepresentativeMethod::START + 1; method < RepresentativeMethod::END; method++)
			{

				std::string temp = Geometry->RepresentativeToString((RepresentativeMethod) method);
				bool is_selected = (currentMethodString == temp);
				if (ImGui::Selectable(temp.c_str(), is_selected))
				{
					currentMethodString = temp;
					currentMethod = (RepresentativeMethod) method;
				}
				if (is_selected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if (currentMethod != Geometry->GetRepresentativeMethod())
		{
			Geometry->SetRepresentativeMethod(currentMethod);
			if (currentDistanceMatrix.name != "Unknown")
			{
				Geometry->Release();
				Geometry->ReleaseObject(Geometry->GetClusteredObjectData());
				Geometry->CalculateHierarchicalClustering();
				Geometry->LoadLineSet(Geometry->GetClusteredObjectData());
				Geometry->Create(g_D3D->GetDevice());
			}
		}

		std::string distanceMetric(currentDistanceMetric);
		if (distanceMetric != currentDistanceMatrix.metric)
		{
			currentDistanceMatrix.metric = distanceMetric;
			g_Scene->SetCurrentDistanceMatrix(currentDistanceMatrix);
			if (currentDistanceMatrix.name != "Unknown")
			{
				Geometry->CalculateClusterReport(g_Scene->ConstructDistanceMatrixPath(currentDistanceMatrix), g_Scene->GetLinkageMode());
			}
		}
	}

	// select cluster size
	int clusterSize = Geometry->GetClusterSize();
	int lineAmount = Geometry->GetTotalLineAmount();
	ImGui::SliderInt("Cluster Count", &clusterSize, 0, lineAmount);

	if (clusterSize != Geometry->GetClusterSize())
	{
		Geometry->SetClusterSize(clusterSize);
		if (currentDistanceMatrix.name != "Unknown")
		{
			Geometry->Release();
			Geometry->ReleaseObject(Geometry->GetClusteredObjectData());
			Geometry->CalculateHierarchicalClustering();
			Geometry->LoadLineSet(Geometry->GetClusteredObjectData());
			Geometry->Create(g_D3D->GetDevice());
		}
	}

	if (ImGui::Button("Set Cluster size", ImVec2(200.f, 20.f)))
	{
		if (currentDistanceMatrix.name != "Unknown")
		{
			Geometry->Release();
			Geometry->ReleaseObject(Geometry->GetClusteredObjectData());
			Geometry->CalculateClusterReport(g_Scene->ConstructDistanceMatrixPath(currentDistanceMatrix), g_Scene->GetLinkageMode());
			Geometry->CalculateHierarchicalClustering();
			Geometry->LoadLineSet(Geometry->GetClusteredObjectData());
			Geometry->Create(g_D3D->GetDevice());
		}
	}

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io->Framerate, io->Framerate);
	ImGui::End();
}

/* write later
void ImguiHelper::ComboBox()
{}
*/
void ImguiHelper::PlotHistogramAndCDF(ID3D11DeviceContext* immediateContext, vislab::RenderTarget2dD3D11& Histogram, vislab::BufferD3D11& CDF, const char* text1, const char* text2)
{
	// plot histogram
	float max = *std::max_element(std::begin(Histogram.Data), std::end(Histogram.Data));
	float* histogram = &Histogram.Data[0];
	ImGui::PlotHistogram("", histogram, HISTOGRAM_BINS, 0, text1, 0.f, max, ImVec2(300.f, 300.f));
	
	ImGui::SameLine();
	
	float* cdf = (float*) &CDF.mData[0];
	ImGui::PlotLines("", cdf, HISTOGRAM_BINS, 0.f, text2, 0.f, 1.0f, ImVec2(300, 300.0f));
}

void ImguiHelper::PlotHistogram(ID3D11DeviceContext* immediateContext, vislab::RenderTarget2dD3D11& Histogram, const char* text)
{
	float max = *std::max_element(std::begin(Histogram.Data), std::end(Histogram.Data));
	float* histogram = &Histogram.Data[0];
	ImGui::PlotHistogram("", histogram, HISTOGRAM_BINS, 0, text, 0.f, max, ImVec2(300.f, 300.f));
}

void ImguiHelper::PlotCDF(ID3D11DeviceContext* immediateContext, vislab::BufferD3D11& CDF, const char* text)
{
	float* cdf = (float*)&CDF.mData[0];
	ImGui::PlotLines("", cdf, HISTOGRAM_BINS, 0.f, text, 0.f, 1.0f, ImVec2(300, 300.0f));
}

bool ImguiHelper::CheckConfig(ImGuiConfigFlags_ flag)
{
	return io->ConfigFlags & flag;
}