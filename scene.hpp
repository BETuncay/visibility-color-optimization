#pragma once

#include <string>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <set>

namespace fs = std::filesystem;

// maybe change this datastructure completely -> seperate lists and/or dont save the path -> recreate path on runtime from name + importance + etc
struct DataDescription {
	std::string name;
	std::string importance;
	std::string scalarColor;
	DataDescription() : name("Unknown"), importance("Unknown"), scalarColor("Unknown")
	{};
};

struct DistanceMatrixDescription {
	std::string name;
	std::string metric;

	DistanceMatrixDescription() : name("Unknown"), metric("Unknown")
	{};
};

class Scene
{
public:
	Scene(std::string path, std::string distanceMatrixPath, std::string seperator, int linkage);
	~Scene();
	
	DataDescription& GetCurrentDataset() { return mCurrentDataset; }
	void SetCurrentDataset(DataDescription value) { mCurrentDataset = value; }
	DataDescription& GetFirstMatchingDatasets(std::string& name);
	DataDescription& GetFirstMatchingDatasets(std::string& name, std::string& importance);
	void RefreshCurrentDatasetImportance();
	void RefreshCurrentDatasetScalarColor();
	void RefreshCurrentDistanceMatrixMetrics();

	DistanceMatrixDescription& GetCurrentDistanceMatrix() { return mCurrentDistanceMatrix; }
	void SetCurrentDistanceMatrix(DistanceMatrixDescription value) { mCurrentDistanceMatrix = value; }
	DistanceMatrixDescription Scene::GetFirstMatchingDistanceMatrix(std::string& name);


	std::set<std::string>& GetDatasetNames() { return mDatasetNames; }
	std::set<std::string> GetCurrentDatasetImportance() { return mCurrentImportances;  }
	std::set<std::string> GetCurrentDatasetScalarColor() { return mCurrentScalarColors; }
	std::vector<std::string> GetCurrentDistanceMetrics() { return mDistanceMetrics; }

	int GetLinkageMode() { return mLinkage; }
	void SetLinkageMode(int value) { mLinkage = value; }


	std::string ConstructDatasetPath(DataDescription& data);
	std::string Scene::ConstructDistanceMatrixPath(DistanceMatrixDescription& data);

private:
	DataDescription ParseDatasetName(const fs::path& path);
	DistanceMatrixDescription Scene::ParseDistanceMatrixName(const fs::path& path);

	std::vector<std::string> SplitString(const std::string& str, const std::string& delimiter);
	std::string JoinVector(const std::vector<std::string>& vec, const std::string& delimiter);

	std::string mDatasetPath;
	std::string mSeperator;
	DataDescription mCurrentDataset;
	std::vector<DataDescription> mDatasets;
	std::set<std::string> mDatasetNames;
	std::set<std::string> mCurrentImportances;
	std::set<std::string> mCurrentScalarColors;

	std::string mDistanceMatrixPath;
	std::vector<DistanceMatrixDescription> mDistanceMatrices;
	DistanceMatrixDescription mCurrentDistanceMatrix;
	std::vector<std::string> mDistanceMetrics;
	int mLinkage;
};
