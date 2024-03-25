#include "lines.hpp"

Lines::Lines(const std::string& path, const std::string& distanceMatrixPath, const RepresentativeMethod repMethod, const int totalNumCPs, const unsigned clusterSize, ID3D11Device* Device) :
	_VbPosition(NULL),
	_VbID(NULL),
	_VbImportance(NULL),
	_VbAlphaWeights(NULL),
	_SrvAlphaWeights(NULL),
	_LineID(NULL),
	_SrvLineID(NULL),
	_VbColor(NULL),
	_TotalNumberOfControlPoints(totalNumCPs),
	mCurrentDataset(path),
	mCurrentDistanceMatrixPath(distanceMatrixPath),
	mClusterSize(clusterSize),
	mRepresentativeMethod(repMethod)
{
	
	Lines::ParseLineData(mCurrentDataset); // parse data and save in mObject
	Lines::CalculateClusterReport(mCurrentDistanceMatrixPath); // set mClusterReport
	Lines::CalculateHierarchicalClustering(); // set mClusterObject using mObject and mClusterReport

	Lines::LoadLineSet(mClusteredObject);
	Lines::Create(Device);
}

Lines::~Lines() 
{
	Release();
	ReleaseObject(mObject);
	ReleaseObject(mClusteredObject);
}

bool Lines::Create(ID3D11Device* Device)
{
	// create the vertex buffers
	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.ByteWidth = (UINT)_Positions.size() * sizeof(XMFLOAT3);
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	D3D11_SUBRESOURCE_DATA initData;
	ZeroMemory(&initData, sizeof(D3D11_SUBRESOURCE_DATA));
	initData.pSysMem = _Positions.data();
	if (FAILED(Device->CreateBuffer(&bufferDesc, &initData, &_VbPosition))) return false;

	bufferDesc.ByteWidth = (UINT)_ID.size() * sizeof(int);
	initData.pSysMem = _ID.data();
	if (FAILED(Device->CreateBuffer(&bufferDesc, &initData, &_VbID))) return false;

	initData.pSysMem = _Importance.data();
	if (FAILED(Device->CreateBuffer(&bufferDesc, &initData, &_VbImportance))) return false;

	initData.pSysMem = _Color.data();
	if (FAILED(Device->CreateBuffer(&bufferDesc, &initData, &_VbColor))) return false;

	// create buffer for the alpha weights
	{
		bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE;
		bufferDesc.ByteWidth = (UINT)_Positions.size() * sizeof(float);
		initData.pSysMem = &_AlphaWeights[0];
		if (FAILED(Device->CreateBuffer(&bufferDesc, &initData, &_VbAlphaWeights))) return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC srv;
		ZeroMemory(&srv, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		srv.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
		srv.BufferEx.NumElements = (UINT)_Positions.size();
		srv.Format = DXGI_FORMAT_R32_TYPELESS;
		if (FAILED(Device->CreateShaderResourceView(_VbAlphaWeights, &srv, &_SrvAlphaWeights))) return false;
	}

	// create current alpha resources
	{
		vislab::BufferDesc desc;
		desc.Num_Elements = (unsigned int) _Positions.size();
		desc.Size_Element = sizeof(float);
		desc.BindFlags = D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlag = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		desc.Format = DXGI_FORMAT_R32_TYPELESS;
		desc.Uav_Flags = D3D11_BUFFER_UAV_FLAG_RAW;
		mVbCurrentAlpha = vislab::BufferD3D11(desc);
		if (!mVbCurrentAlpha.D3DCreate(Device))
			return false;
	}

	for (int p = 0; p < 2; ++p)
	{
		vislab::BufferDesc desc;
		desc.Num_Elements = _TotalNumberOfControlPoints;
		desc.Size_Element = sizeof(unsigned int);
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlag = 0;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		desc.Format = DXGI_FORMAT_R32_TYPELESS;
		desc.Uav_Flags = D3D11_BUFFER_UAV_FLAG_RAW;
		mAlphaBuffer[p] = vislab::BufferD3D11(desc);
		if (!mAlphaBuffer[p].D3DCreate(Device))
			return false;
	}

	{
		unsigned int NUM_ELEMENTS = _TotalNumberOfControlPoints;
		D3D11_BUFFER_DESC bufDesc;
		ZeroMemory(&bufDesc, sizeof(D3D11_BUFFER_DESC));
		bufDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
		bufDesc.ByteWidth = NUM_ELEMENTS * sizeof(unsigned int);
		bufDesc.CPUAccessFlags = 0;
		bufDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
		bufDesc.StructureByteStride = sizeof(unsigned int);
		bufDesc.Usage = D3D11_USAGE_DEFAULT;

		D3D11_SUBRESOURCE_DATA initData;
		ZeroMemory(&initData, sizeof(D3D11_SUBRESOURCE_DATA));
		initData.pSysMem = _ControlPointLineIndices.data();
		if (FAILED(Device->CreateBuffer(&bufDesc, &initData, &_LineID))) return false;

		D3D11_SHADER_RESOURCE_VIEW_DESC srv;
		ZeroMemory(&srv, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		srv.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		srv.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
		srv.BufferEx.NumElements = NUM_ELEMENTS;
		srv.Format = DXGI_FORMAT_R32_TYPELESS;
		if (FAILED(Device->CreateShaderResourceView(_LineID, &srv, &_SrvLineID))) return false;
	}

	return true;
}

void Lines::Release()
{
	if (_VbPosition)		_VbPosition->Release();			_VbPosition = NULL;
	if (_VbID)				_VbID->Release();				_VbID = NULL;
	if (_VbImportance)		_VbImportance->Release();		_VbImportance = NULL;
	if (_VbAlphaWeights)	_VbAlphaWeights->Release();		_VbAlphaWeights = NULL;
	if (_SrvAlphaWeights)	_SrvAlphaWeights->Release();	_SrvAlphaWeights = NULL;
	mVbCurrentAlpha.D3DRelease();
	for (int p = 0; p < 2; ++p) {
		mAlphaBuffer[p].D3DRelease();
	}
	if (_LineID)			_LineID->Release();				_LineID = NULL;
	if (_SrvLineID)			_SrvLineID->Release();			_SrvLineID = NULL;
	if (_VbColor)			_VbColor->Release();			_VbColor = NULL;

	// clear these arrays for imgui reuse
	_Importance.clear();
	_Color.clear();
	_LineLengths.clear();
	_NumberOfControlPointsOfLine.clear();
	_AlphaWeights.clear();
	_ControlPointLineIndices.clear();
}

void Lines::ReleaseObject(ObjectData& object)
{
	object.lines.clear();
	object.importance.clear();
	object.scalarColor.clear();
}

void Lines::DrawHQ(ID3D11DeviceContext* ImmediateContext)
{
	ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

	ID3D11Buffer* vbs[] = { _VbPosition, _VbID, mVbCurrentAlpha.GetBuf(), _VbColor };
	UINT strides[] = { sizeof(float) * 3, sizeof(int), sizeof(float), sizeof(float) };
	UINT offsets[] = { 0, 0, 0, 0 };
	ImmediateContext->IASetVertexBuffers(0, 4, vbs, strides, offsets);
	ImmediateContext->Draw(GetTotalNumberOfVertices() - 2, 0);
}

void Lines::DrawLowRes(ID3D11DeviceContext* ImmediateContext)
{
	ImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);

	ID3D11Buffer* vbs[] = { _VbPosition, _VbID, _VbImportance, _VbAlphaWeights };
	UINT strides[] = { sizeof(float) * 3, sizeof(int), sizeof(float), sizeof(float) };
	UINT offsets[] = { 0, 0, 0, 0 };
	ImmediateContext->IASetVertexBuffers(0, 4, vbs, strides, offsets);
	ImmediateContext->Draw(GetTotalNumberOfVertices() - 2, 0);
}

// set the value of mObject
void Lines::ParseLineData(const std::string& path)
{
	// Parse input obj file 
	static const int OBJ_ZERO_BASED_SHIFT = -1;

	mObject.vertexCount = 0;

	XMFLOAT3 last(0, 0, 0);
	std::vector<XMFLOAT3> vertices;
	std::vector<float> vertexImportances;
	std::vector<float> vertexScalarColors;
	std::ifstream myfile(path);
	if (myfile.is_open())
	{
		std::string line;
		while (myfile.good())
		{
			std::getline(myfile, line);
			if (line.size() < 2) continue;
			if (line[0] == 'v' && line[1] == ' ')	// read in vertex
			{
				XMFLOAT3 vertex;
				if (sscanf_s(line.c_str(), "v %f %f %f", &vertex.x, &vertex.y, &vertex.z) == 3)
					vertices.push_back(vertex);
			}
			else if (line[0] == 'v' && line[1] == 't')	// read in tex coords (aka importance and scalarColor)
			{
				float imp, col;
				if (sscanf_s(line.c_str(), "vt %f %f", &imp, &col) == 1)
				{
					col = imp; // if no color scalar is given just use the importance instead
				}
				vertexImportances.push_back(imp);
				vertexScalarColors.push_back(col);
			}
			else if (line[0] == 'l') // read in line indices
			{
				std::stringstream stream(line);
				std::string lead;
				stream >> lead;
				int index;
				Line vline;
				std::vector<float> vimportance;
				std::vector<float> vscalarColor;
				float noise = 0.f; // 0.01f + rand() / (float)(RAND_MAX) * 0.2f;
				while (stream >> index)
				{
					// was originally using XMVector3LengthSq instead of XMVector3Length here -> incase errors happen
					float dist = Lines::CalculatePointDistance(&vertices[index + OBJ_ZERO_BASED_SHIFT], &last);
					if (0.0001f < dist && dist < 999999999.f)
					{
						last = vertices[index + OBJ_ZERO_BASED_SHIFT];
						vline.push_back(vertices[index + OBJ_ZERO_BASED_SHIFT]);
						mObject.vertexCount += vline.size();

						if (index + OBJ_ZERO_BASED_SHIFT < vertexImportances.size())
						{
							vimportance.push_back(vertexImportances[index + OBJ_ZERO_BASED_SHIFT] + noise);
							vscalarColor.push_back(vertexScalarColors[index + OBJ_ZERO_BASED_SHIFT]);
						}
					}
				}
				mObject.lines.push_back(vline);
				mObject.importance.push_back(vimportance);
				mObject.scalarColor.push_back(vscalarColor);
			}
		}
		myfile.close();
	}
}

float Lines::CalculateLineLength(const Line& line)
{
	float length = 0;
	for (unsigned int id = 0; id < line.size() - 1; ++id)
	{
		length += Lines::CalculatePointDistance(&line[id], &line[id + 1]);
	}
	return length;
}

float Lines::CalculatePointDistance(const XMFLOAT3* a, const XMFLOAT3* b)
{
	XMVECTOR point0 = XMLoadFloat3(a);
	XMVECTOR point1 = XMLoadFloat3(b);
	float distance = 0;
	XMStoreFloat(&distance, XMVector3Length(point0 - point1));
	return distance;
}

void Lines::StorePositionsIds(const std::vector<Line>& lines, const unsigned int totalNumPoints)
{
	_Positions.resize(totalNumPoints);
	_ID.resize(totalNumPoints);

	unsigned int offset = 0;
	unsigned int lineID = 0;
	for (auto itLine = lines.begin(); itLine != lines.end(); ++itLine)
	{
		memcpy_s(&(_Positions)[offset], (totalNumPoints - offset) * sizeof(XMFLOAT3), &((*itLine)[0]), itLine->size() * sizeof(XMFLOAT3));
		for (unsigned int i = 0; i < itLine->size(); ++i)
			_ID[i + offset] = lineID;
		offset += (int)itLine->size();
		lineID++;
	}
}

void Lines::StoreImportanceScalarColor(const ObjectData& clusteredObject)
{
	_Importance.resize(clusteredObject.vertexCount);
	_Color.resize(clusteredObject.vertexCount);
	unsigned int offset = 0;
	for (const std::vector<float>& imp : clusteredObject.importance)
	{
		memcpy_s(&(_Importance)[offset], imp.size() * sizeof(float), &(imp[0]), imp.size() * sizeof(float));
		offset += (int)imp.size();
	}

	offset = 0;
	for (const std::vector<float>& col : clusteredObject.scalarColor)
	{
		memcpy_s(&(_Color)[offset], col.size() * sizeof(float), &(col[0]), col.size() * sizeof(float));
		offset += (int)col.size();
	}
}

std::vector<int> Lines::DistributePolylines(const unsigned int lines_amount, const float accumLineLength, const unsigned totalNumberOfControlPoints, const std::vector<float>& lineLengths)
{
	assert(totalNumberOfControlPoints * 2 > lines_amount);
	std::vector<int> numberOfControlPointsOfLine(lines_amount);

	// calculate the average length of a polyline segment
	float lengthPerPatch = accumLineLength / totalNumberOfControlPoints;

	// we first make sure that lines shorter than this length receive two polyline segments
	unsigned int remPatches = totalNumberOfControlPoints;
	float remLength = accumLineLength;
	for (int lineId = 0; lineId < lines_amount; ++lineId)
	{
		float length = lineLengths[lineId];
		if (length <= lengthPerPatch)
		{
			numberOfControlPointsOfLine[lineId] = 2;
			remPatches -= 2;
			remLength -= length;
		}
	}

	// the remaining polyline segments are distributed among the other lines
	std::map<float, unsigned int> remainder;
	unsigned int assignedPatches = 0;
	for (unsigned int lineId = 0; lineId < lines_amount; ++lineId)
	{
		float length = lineLengths[lineId];
		if (length > lengthPerPatch)
		{
			float numPatches = length / remLength * remPatches;
			numberOfControlPointsOfLine[lineId] = (unsigned int)numPatches;
			assignedPatches += (int)numPatches;
			remainder.insert(std::pair<float, unsigned int>(numPatches - (unsigned int)numPatches, lineId));
		}
	}

	// due to rounding a few segments are not yet assigned. do so now.
	assert(assignedPatches <= remPatches);
	while (assignedPatches < remPatches)
	{
		// get the patch with the largest remainder
		auto itBiggest = --remainder.end();
		int biggestRemLineId = itBiggest->second;
		remainder.erase(itBiggest);
		numberOfControlPointsOfLine[biggestRemLineId] += 1;
		assignedPatches += 1;
	}

	return numberOfControlPointsOfLine;
}

std::vector<float> Lines::ComputeAlphaWeights(const std::vector<Line> lines, const unsigned int totalNumPoints, const std::vector<float>& lineLengths, std::vector<int>& numberOfControlPointsOfLine)
{
	std::vector<float> alphaWeight(totalNumPoints);
	int offset = 0;
	int lineId = 0;
	int cpOffset = 0;
	for (auto itLine = lines.begin(); itLine != lines.end(); ++itLine, lineId++)
	{
		float currLength = 0;
		float lineLength = lineLengths[lineId];
		alphaWeight[offset] = (float)cpOffset; //_NumControlPointsPerLine * lineId;
		offset++;

		int numCp = numberOfControlPointsOfLine[lineId];
		for (unsigned int id = 0; id < itLine->size() - 1; ++id)
		{
			currLength += Lines::CalculatePointDistance(&itLine->at(id), &itLine->at(id + 1));
			alphaWeight[offset] = std::min(currLength / lineLength * (numCp - 1), numCp - 1 - 0.0001f);
			alphaWeight[offset] += cpOffset;

			offset++;
		}
		cpOffset += numCp;
	}
	if (offset != totalNumPoints)
		throw new std::exception("Offset mismatch.");

	return alphaWeight;
}

void Lines::LoadLineSet(const ObjectData& object)
{
	
	int lines_amount = object.lines.size();
	_LineLengths.resize(lines_amount);

	// calculate total line length and total amount of points
	float accumLineLength = 0;
	unsigned int totalNumPoints = 0;
	for (unsigned int line_id = 0; line_id < lines_amount; line_id++)
	{
		_LineLengths[line_id] = Lines::CalculateLineLength(object.lines[line_id]);
		accumLineLength += _LineLengths[line_id];
		totalNumPoints += object.lines[line_id].size();
	}

	// store importance / scalarColor data in linear memory (_Importance, _Color)
	StoreImportanceScalarColor(object);

	// store position data and IDs in linear memory -> use later for gpu
	Lines::StorePositionsIds(object.lines, totalNumPoints);

	// Distribute polyline segments (here sometimes called control points) among the lines so that they are roughly equally-sized.
	_NumberOfControlPointsOfLine = Lines::DistributePolylines(lines_amount, accumLineLength, _TotalNumberOfControlPoints, _LineLengths);

	// Compute the blending weight parameterization // compute the (alpha) control weights
	_AlphaWeights = Lines::ComputeAlphaWeights(object.lines, totalNumPoints, _LineLengths, _NumberOfControlPointsOfLine);

	{
		_ControlPointLineIndices.resize(_TotalNumberOfControlPoints);
		int lineID = 0;
		int cpID = 0;
		for (int numCP : _NumberOfControlPointsOfLine)
		{
			for (int i = 0; i < numCP; ++i)
				_ControlPointLineIndices[cpID++] = lineID;

			lineID++;
		}
	}
}

void Lines::CalculateHierarchicalClustering()
{
	if (mClusterSize == 0 || mClusterSize > mObject.lines.size())
	{
		mClusteredObject = mObject;
		return;
	}
	
	alglib::integer_1d_array cidx;
	alglib::integer_1d_array cz;
	alglib::clusterizergetkclusters(mClusterReport, mClusterSize, cidx, cz);

	mClusteredObject.lines.resize(mClusterSize);
	mClusteredObject.importance.resize(mClusterSize);
	mClusteredObject.scalarColor.resize(mClusterSize);

	PickClusterRepresentatives(mRepresentativeMethod, cidx, 1.0, 1.0);
}

void Lines::PickClusterRepresentatives(RepresentativeMethod method, alglib::integer_1d_array& clusterMapping, double importanceWeight, double scalarWeight)
{
	// pick representatives for each cluster
	switch (method)
	{
	case RepresentativeMethod::FirstLine:
	{
		FirstRepresentatives(clusterMapping);
		break;
	}
	case RepresentativeMethod::MeanLine:
	{
		MeanRepresentatives(clusterMapping, importanceWeight, scalarWeight);
		break;
	}
	case RepresentativeMethod::LeastDistanceCurve:
	{
		LeastDistanceRepresentatives(clusterMapping);
		break;
	}
	case RepresentativeMethod::MostImportantCurve:
	{
		MostImportantRepresentatives(clusterMapping);
		break;
	}
	default:
		break;
	}
}

void Lines::FirstRepresentatives(alglib::integer_1d_array& clusterMapping)
{
	unsigned int vertexCount = 0;
	std::set<alglib::ae_int_t> alreadySet;
	for (unsigned int id = 0; id < clusterMapping.length(); id++)
	{
		alglib::ae_int_t clusterId = clusterMapping[id];
		if (alreadySet.count(clusterId) == 0)
		{
			mClusteredObject.lines[clusterId] = mObject.lines[id];
			mClusteredObject.importance[clusterId] = mObject.importance[id];
			mClusteredObject.scalarColor[clusterId] = mObject.scalarColor[id];
			alreadySet.insert(clusterId);
			vertexCount += mObject.lines[id].size();
		}
	}
	mClusteredObject.vertexCount = vertexCount;
}

// lemme think about this
// line:  .---.---.---.  (point = vertex)
// each vertex has a improtance/scalarColor value
// I want to calculate mean line per cluster
// Use Monotone Parametrization to get "equal" positions on lines -> dont do it, leads to weird results :( -> just use points directly up to min line length
// calculate sum of lines and divide by line count
// after that -> add importance / scalarColor weights 
// or just dont weight by importance/scalarColor ?
void Lines::MeanRepresentatives(alglib::integer_1d_array& clusterMapping, double importanceWeight, double scalarWeight)
{
	// iterate over all lines and determine the precision for each cluster mean line
	std::vector<unsigned> numPoints(mClusterSize, UINT_MAX);		// depends on lowest line resolution of the cluster
	for (unsigned int id = 0; id < clusterMapping.length(); id++)
	{
		numPoints[clusterMapping[id]] = std::min(numPoints[clusterMapping[id]], (unsigned) mObject.lines[id].size());
	}

	// initialize data
	std::vector<Line> meanLines(mClusterSize);							// mean line of cluster
	std::vector<std::vector<float>> meanImportances(mClusterSize);		// mean line importance values
	std::vector<std::vector<float>> meanScalarColor(mClusterSize);		// mean line scalarColor values
	std::vector<unsigned> clusterSize(mClusterSize, 0);					// num of lines contained in cluster
	for (unsigned i = 0; i < mClusterSize; i++)
	{
		meanLines[i].resize(numPoints[i]);
		meanImportances[i].resize(numPoints[i]);
		meanScalarColor[i].resize(numPoints[i]);
	}

	// for each line -> add line to line sum
	for (unsigned int id = 0; id < clusterMapping.length(); id++)
	{
		const unsigned clusterId = clusterMapping[id];
		clusterSize[clusterId] += 1;
		for (unsigned point = 0; point < numPoints[clusterId]; point++)
		{
			// line calculation
			XMVECTOR currVector = XMLoadFloat3(&mObject.lines[id][point]);
			XMVECTOR sumVector = XMLoadFloat3(&meanLines[clusterId][point]);
			sumVector = XMVectorAdd(sumVector, currVector);
			XMStoreFloat3(&meanLines[clusterId][point], sumVector);

			// importance calculation
			float importanceSample = mObject.importance[id][point];
			meanImportances[clusterId][point] += importanceSample;

			// scalarColor calculation
			float scalarColorSample = mObject.scalarColor[id][point];
			meanScalarColor[clusterId][point] += scalarColorSample;
		}
	}

	// calculate the mean line per cluster -> divide lineSum by the amount of lines per cluster
	for (unsigned cluster = 0; cluster < mClusterSize; cluster++)
	{
		float factor = 1.f / (float) (clusterSize[cluster]);
		for (unsigned point = 0; point < numPoints[cluster]; point++)
		{
			Line& meanLine = meanLines[cluster];
			XMVECTOR meanVector = XMLoadFloat3(&meanLine[point]);
			meanVector *= factor;
			XMStoreFloat3(&meanLine[point], meanVector);

			meanImportances[cluster][point] *= factor;
			meanScalarColor[cluster][point] *= factor;
		}
	}

	mClusteredObject.lines = meanLines;
	mClusteredObject.importance = meanImportances;
	mClusteredObject.scalarColor = meanScalarColor;
	unsigned vertexCount = 0;
	for (unsigned num : numPoints)
	{
		vertexCount += num;
	}
	mClusteredObject.vertexCount = vertexCount;
}

/*
void Lines::MeanRepresentatives(alglib::integer_1d_array& clusterMapping, double importanceWeight, double scalarWeight)
{
	// iterate over all lines and determine the precision for each cluster mean line
	std::vector<unsigned> numPoints(mClusterSize, UINT_MAX);		// depends on lowest line resolution of the cluster
	for (unsigned int id = 0; id < clusterMapping.length(); id++)
	{
		numPoints[clusterMapping[id]] = std::min(numPoints[clusterMapping[id]], (unsigned)mObject.lines[id].size());
	}

	// initialize data
	std::vector<Line> meanLines(mClusterSize);							// mean line of cluster
	std::vector<std::vector<float>> meanImportances(mClusterSize);		// mean line importance values
	std::vector<std::vector<float>> meanScalarColor(mClusterSize);		// mean line scalarColor values
	std::vector<unsigned> clusterSize(mClusterSize, 0);					// num of lines contained in cluster
	for (unsigned i = 0; i < mClusterSize; i++)
	{
		meanLines[i].resize(numPoints[i]);
		meanImportances[i].resize(numPoints[i]);
		meanScalarColor[i].resize(numPoints[i]);
	}

	// sum over all lines
	for (unsigned int id = 0; id < clusterMapping.length(); id++)
	{
		const unsigned clusterId = clusterMapping[id];
		clusterSize[clusterId] += 1;

		Line& currLine = mObject.lines[id];
		std::vector<float>& currImportance = mObject.importance[id];
		std::vector<float>& currScalarColor = mObject.scalarColor[id];
		Line& sumLine = meanLines[clusterId];
		std::vector<float>& sumImportance = meanImportances[clusterId];
		std::vector<float>& sumScalarColor = meanScalarColor[clusterId];
		// add current line to cluster sum
		for (unsigned point = 0; point < numPoints[clusterId]; point++)
		{
			float idx_float = (float)(point / (numPoints[clusterId] - 1.f)) * (currLine.size() - 1.f);
			unsigned idx = std::floor(idx_float);
			unsigned idx_next = std::min(idx + 1, numPoints[clusterId] - 1);
			float fract = idx_float - idx;

			// line calculation
			XMVECTOR currVectorA = XMLoadFloat3(&currLine[idx]);
			XMVECTOR currVectorB = XMLoadFloat3(&currLine[idx_next]);
			XMVECTOR sumVector = XMLoadFloat3(&sumLine[point]);

			currVectorA = currVectorA * (1 - fract);
			currVectorB = currVectorB * fract;

			sumVector = XMVectorAdd(sumVector, currVectorA);
			sumVector = XMVectorAdd(sumVector, currVectorB);
			XMStoreFloat3(&sumLine[point], sumVector);

			// importance calculation
			float importanceSample = currImportance[idx] * (1.f - fract) + currImportance[idx_next] * fract;
			sumImportance[point] += importanceSample;

			// scalarColor calculation
			float scalarColorSample = currScalarColor[idx] * (1.f - fract) + currScalarColor[idx_next] * fract;
			sumScalarColor[point] += scalarColorSample;
		}
	}

	// calculate the mean line per cluster -> divide lineSum by the amount of lines per cluster
	for (unsigned cluster = 0; cluster < mClusterSize; cluster++)
	{
		float factor = 1.f / (float)(clusterSize[cluster]);
		for (unsigned point = 0; point < numPoints[cluster]; point++)
		{
			Line& meanLine = meanLines[cluster];
			XMVECTOR meanVector = XMLoadFloat3(&meanLine[point]);
			meanVector *= factor;
			XMStoreFloat3(&meanLine[point], meanVector);

			meanImportances[cluster][point] *= factor;
			meanScalarColor[cluster][point] *= factor;
		}
	}

	mClusteredObject.lines = meanLines;
	mClusteredObject.importance = meanImportances;
	mClusteredObject.scalarColor = meanScalarColor;
	unsigned vertexCount = 0;
	for (unsigned num : numPoints)
	{
		vertexCount += num;
	}
	mClusteredObject.vertexCount = vertexCount;
}
*/


// read distance matrix
// read rows of all lines in a cluster
// pick line with minimum distance values according to some metric
// Importance / ScalarColor scaling is already included in the distance matrix
// -----------------------
// example distance matrix
// 0 1 2 1
// 1 0 3 2
// 2 3 0 5
// 1 2 5 0
// cluster = 1, 2, 4
// totalDistance 1 = 0 + 1 + 1
// totalDistance 2 = 1 + 0 + 2
// totalDistance 3 = 1 + 2 + 0
void Lines::LeastDistanceRepresentatives(alglib::integer_1d_array& clusterMapping)
{
	// for each cluster -> get all line ids
	std::vector<std::vector<unsigned>> clusterIDs(mClusterSize);
	for (unsigned lineID = 0; lineID < clusterMapping.length(); lineID++)
	{
		clusterIDs[clusterMapping[lineID]].push_back(lineID);
	}

	// for each line -> calculate total distance to all other lines in its cluster
	std::vector<float> totalLineClusterDistance(clusterMapping.length(), 0.f);
	for (unsigned int lineID = 0; lineID < clusterMapping.length(); lineID++)
	{
		unsigned clusterID = clusterMapping[lineID];
		for (unsigned otherLineID : clusterIDs[clusterID])
		{
			totalLineClusterDistance[lineID] += mDistanceMatrix(clusterID, otherLineID);
		}
	}

	// for each cluster -> pick representative line
	unsigned int vertexCount = 0;
	for (unsigned clusterID = 0; clusterID < mClusterSize; clusterID++)
	{
		// pick minimum line from the cluster
		unsigned representativeID; // this will always be initalized
		float minValue = UINT_MAX;
		for (unsigned lineID : clusterIDs[clusterID])
		{
			if (totalLineClusterDistance[lineID] < minValue)
			{
				minValue = totalLineClusterDistance[lineID];
				representativeID = lineID;
			}
		}
		mClusteredObject.lines[clusterID] = mObject.lines[representativeID];
		mClusteredObject.importance[clusterID] = mObject.importance[representativeID];
		mClusteredObject.scalarColor[clusterID] = mObject.scalarColor[representativeID];
		vertexCount += mObject.lines[representativeID].size();
	}

	mClusteredObject.vertexCount = vertexCount;
}

void Lines::MostImportantRepresentatives(alglib::integer_1d_array& clusterMapping)
{
	// calculate the point weigths for each line w_i = |p_i - p_{i-1}| + |p_i - p{i+1}|
	std::vector<std::vector<float>> pointWeights(mObject.lines.size());
	for (unsigned i = 0; i < mObject.lines.size(); i++)
	{
		pointWeights[i].resize(mObject.lines[i].size());
		pointWeights[i][0] = CalculatePointDistance(&mObject.lines[i][0], &mObject.lines[i][1]);
		for (unsigned point = 1; point < mObject.lines[i].size() - 1; point++) 
		{
			pointWeights[i][point] = CalculatePointDistance(&mObject.lines[i][point], &mObject.lines[i][point - 1]) + CalculatePointDistance(&mObject.lines[i][point], &mObject.lines[i][point + 1]);
		}
		pointWeights[i][mObject.lines[i].size() - 1] = CalculatePointDistance(&mObject.lines[i][mObject.lines[i].size() - 1], &mObject.lines[i][mObject.lines[i].size() - 2]);
	}

	// calculate the total importance per cluster
	std::vector<double> totalImportance(mObject.lines.size(), 0.0);
	for (unsigned i = 0; i < mObject.lines.size(); i++)
	{
		// calculate sum of weights of this line
		double totalWeight = 0.0;
		for (unsigned j = 0; j < mObject.lines[i].size(); j++)
		{
			totalWeight += pointWeights[i][j];
		}
		
		for (unsigned j = 0; j < mObject.lines[i].size(); j++)
		{
			totalImportance[i] += (mObject.importance[i][j] * pointWeights[i][j]) / totalWeight;
		}
	}
	
	// for each cluster -> get all line ids
	std::vector<std::vector<unsigned>> clusterIDs(mClusterSize);
	for (unsigned lineID = 0; lineID < clusterMapping.length(); lineID++)
	{
		clusterIDs[clusterMapping[lineID]].push_back(lineID);
	}

	// for each cluster -> pick maximum importance line
	unsigned int vertexCount = 0;
	for (unsigned clusterID = 0; clusterID < mClusterSize; clusterID++)
	{
		// pick minimum line from the cluster
		float maxValue = -DBL_MAX;
		unsigned representativeID;  
		for (unsigned lineID : clusterIDs[clusterID])
		{
			if (totalImportance[lineID] > maxValue)
			{
				maxValue = totalImportance[lineID];
				representativeID = lineID;
			}
		}
		mClusteredObject.lines[clusterID] = mObject.lines[representativeID];
		mClusteredObject.importance[clusterID] = mObject.importance[representativeID];
		mClusteredObject.scalarColor[clusterID] = mObject.scalarColor[representativeID];
		vertexCount += mObject.lines[representativeID].size();
	}

	mClusteredObject.vertexCount = vertexCount;
}

alglib::real_2d_array Lines::LoadDistanceMatrix(const std::string& filename)
{
	alglib::real_2d_array d;
	std::ifstream in(filename);
	if (in.is_open())
	{
		std::string s;
		in >> s;
		d = s.c_str();
	}
	return d;
}

void Lines::CalculateClusterReport(const std::string& distanceMatrixPath, int linkageType)
{
	// alglib clustering
	alglib::clusterizerstate s;
	//alglib::ahcreport rep;
	mDistanceMatrix = Lines::LoadDistanceMatrix(distanceMatrixPath);

	alglib::clusterizercreate(s);
	alglib::clusterizersetahcalgo(s, linkageType);
	alglib::clusterizersetdistances(s, mDistanceMatrix, true);
	alglib::clusterizerrunahc(s, mClusterReport);
}