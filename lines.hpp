#pragma once

#include "math.hpp"
#include <d3d11.h>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include "buffer.hpp"
#include "stdafx.h"
#include "dataanalysis.h"

typedef std::vector<XMFLOAT3> Line;

// make this different -> vector<ObjectData> should be used instead
struct ObjectData {
	std::vector<Line> lines;
	std::vector<std::vector<float>> importance;
	std::vector<std::vector<float>> scalarColor;
	unsigned int vertexCount;
};

enum RepresentativeMethod {
	START,
	FirstLine,					// pick first line found while iterating
	MeanLine,					// calculate the mean line from cluster -> weight with importance / scalarColor
	LeastDistanceCurve,			// pick curve with least distance to all other curves
	MostImportantCurve,			// pick curve with largest importance value
	END
};

class Lines
{
public:

	Lines(const std::string& path, const std::string& distanceMatrixPath, const RepresentativeMethod repMethod, const int totalNumCPs, const unsigned clusterSize, ID3D11Device* Device);
	~Lines();

	bool Create(ID3D11Device* Device);
	void Release();
	void DrawHQ(ID3D11DeviceContext* ImmediateContext);
	void DrawLowRes(ID3D11DeviceContext* ImmediateContext);
	void LoadLineSet(const ObjectData& object);
	void ParseLineData(const std::string& path);
	void CalculateHierarchicalClustering();
	void CalculateClusterReport(const std::string& distanceMatrixPath, int linkageType = 0);
	void PickClusterRepresentatives(RepresentativeMethod method, alglib::integer_1d_array& clusterMapping, double importanceWeight, double scalarWeight);
	void FirstRepresentatives(alglib::integer_1d_array& clusterMapping);
	void MeanRepresentatives(alglib::integer_1d_array& clusterMapping, double importanceWeight, double scalarWeight);
	void LeastDistanceRepresentatives(alglib::integer_1d_array& clusterMapping);
	void MostImportantRepresentatives(alglib::integer_1d_array& clusterMapping);
	void ReleaseObject(ObjectData& object);

	vislab::BufferD3D11& GetCurrentAlpha() { return mVbCurrentAlpha; }
	std::array<vislab::BufferD3D11, 2>& GetAlpha() { return mAlphaBuffer; }
	ID3D11ShaderResourceView* GetSrvAlphaWeights() { return _SrvAlphaWeights; }
	ID3D11ShaderResourceView* GetSrvLineID() { return _SrvLineID; }

	int GetTotalNumberOfControlPoints() const { return _TotalNumberOfControlPoints; }
	int GetTotalNumberOfVertices() const { return (int)_Positions.size(); }
	std::string& GetCurrentDataSet() { return mCurrentDataset; }
	void SetCurrentDataSet(std::string value) { mCurrentDataset = value; }
	unsigned GetClusterSize() const { return mClusterSize; }
	void SetClusterSize(unsigned int value) { mClusterSize = value; }

	ObjectData& GetObjectData() { return mObject; }
	ObjectData& GetClusteredObjectData() { return mClusteredObject; }
	unsigned GetTotalLineAmount() { return mObject.lines.size(); }

	RepresentativeMethod GetRepresentativeMethod() { return mRepresentativeMethod; }
	void SetRepresentativeMethod(RepresentativeMethod value) { mRepresentativeMethod = value; }
	std::string RepresentativeToString(RepresentativeMethod method) 
	{
		switch (method) 
		{
		case RepresentativeMethod::FirstLine:			return	"First Line";
		case RepresentativeMethod::MeanLine:			return	"Mean Line";
		case RepresentativeMethod::LeastDistanceCurve:	return	"Least Distance Curve";
		case RepresentativeMethod::MostImportantCurve:	return	"MostImportantCurve";
		}
	}


private:
	float CalculateLineLength(const Line& line);
	float CalculatePointDistance(const XMFLOAT3* a, const XMFLOAT3* b);
	void StorePositionsIds(const std::vector<Line>& lines, const unsigned int totalNumPoints);
	void StoreImportanceScalarColor(const ObjectData& clusteredObject);
	std::vector<int> DistributePolylines(const unsigned int lines_amount, const float accumLineLength, const unsigned totalNumberOfControlPoints, const std::vector<float>& lineLengths);
	std::vector<float> ComputeAlphaWeights(const std::vector<Line> lines, const unsigned int totalNumPoints, const std::vector<float>& lineLengths, std::vector<int>& numberOfControlPointsOfLine);
	alglib::real_2d_array LoadDistanceMatrix(const std::string& filename);

	int _TotalNumberOfControlPoints;
	unsigned int mClusterSize;

	ID3D11Buffer* _VbPosition;
	ID3D11Buffer* _VbID;
	ID3D11Buffer* _VbImportance;
	ID3D11Buffer* _VbAlphaWeights;			// blending weights (basically the position between control points)
	vislab::BufferD3D11 mVbCurrentAlpha;	// alpha stored with the vertex buffer

	ID3D11ShaderResourceView* _SrvAlphaWeights;

	std::array<vislab::BufferD3D11, 2> mAlphaBuffer; // alpha at the control points -> desired alpha value -> we smoothly fade to it.

	ID3D11Buffer* _LineID;							// stores for every control point the lineID (used for smoothing)
	ID3D11ShaderResourceView* _SrvLineID;

	std::vector<Vec3f> _Positions;
	std::vector<int> _ID;
	std::vector<float> _Importance;
	std::vector<float> _AlphaWeights;	// Blending weight parameterization

	std::vector<float> _LineLengths;
	std::vector<int> _NumberOfControlPointsOfLine;
	std::vector<unsigned int> _ControlPointLineIndices;


	ID3D11Buffer* _VbColor;			// color scalar value
	std::vector<float> _Color;
	std::string mCurrentDataset;				// this should be in scene.cpp
	std::string mCurrentDistanceMatrixPath;		// used for clustering

	ObjectData mObject;							// save the obj file
	alglib::real_2d_array mDistanceMatrix;		// save distance matrix
	ObjectData mClusteredObject;				// save the obj file
	alglib::ahcreport mClusterReport;			// save report to calculate clusters

	RepresentativeMethod mRepresentativeMethod;
};