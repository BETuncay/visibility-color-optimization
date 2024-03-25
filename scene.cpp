#include "scene.hpp"


Scene::Scene(std::string path, std::string distanceMatrixPath, std::string seperator, int linkage): 
	mDatasetPath(path), 
	mSeperator(seperator), 
	mDistanceMatrixPath(distanceMatrixPath),
	mLinkage(linkage)
{
    for (const auto& entry : fs::directory_iterator(mDatasetPath))
	{
		const fs::path path = entry.path();
		const DataDescription desc = ParseDatasetName(path);
		mDatasets.push_back(desc);
		mDatasetNames.insert(desc.name);
	}
	if (!mDatasets.empty())
	{
		mCurrentDataset = mDatasets[0];
		RefreshCurrentDatasetImportance();
		RefreshCurrentDatasetScalarColor();
	}
	

	// if distanceMatrix path exists then add them to data structures
	if (fs::exists(fs::path(mDistanceMatrixPath)))
	{
		for (const auto& entry : fs::directory_iterator(mDistanceMatrixPath))
		{
			const fs::path path = entry.path();
			const DistanceMatrixDescription desc = ParseDistanceMatrixName(path);
			mDistanceMatrices.push_back(desc);
		}
		if (!mDistanceMatrices.empty())
		{
			mCurrentDistanceMatrix = mDistanceMatrices[0];
			RefreshCurrentDistanceMatrixMetrics();
		}
	}
}
Scene::~Scene() {}

// example path = "benzene---curvature---lineLength---1000.obj"
DataDescription Scene::ParseDatasetName(const fs::path& path)
{
	std::vector<std::string> parsed = Scene::SplitString(path.filename().string(), mSeperator);
	DataDescription desc;
	if (parsed.size() == 1)
	{
		desc.name = parsed[0];
	}
	
	if (parsed.size() == 3)
	{
		desc.name = parsed[0];
		desc.importance = parsed[1];
		desc.scalarColor = parsed[2];
	}
	return desc;
}

// example path = "benzene_metric_somestuff.dist"
DistanceMatrixDescription Scene::ParseDistanceMatrixName(const fs::path& path)
{
	std::vector<std::string> parsed = Scene::SplitString(path.filename().string(), mSeperator);
	DistanceMatrixDescription desc;
	
	if (parsed.empty())
	{
		return desc; 
	}
	
	if (parsed.size() == 1)
	{
		desc.name = parsed[0];
		return desc;
	}

	if (parsed.size() > 1)
	desc.name = parsed[0];
	
	parsed.erase(parsed.begin());	
	desc.metric = Scene::JoinVector(parsed, mSeperator);
	return desc;
}

std::vector<std::string> Scene::SplitString(const std::string& str, const std::string& delimiter)
{
	if (str.empty())
		return {};

	// remove file extension
	std::size_t found = str.find_last_of(".");
	std::string temp = str;
	if (found != std::string::npos)
	{
		temp = temp.substr(0, found);
	}

	// find all delimiter location
	std::vector<std::size_t> delimiterLocations;
	std::size_t index = -1;
	while (true)
	{
		index = temp.find(delimiter, index + 1);
		if (index != std::string::npos)
		{
			delimiterLocations.push_back(index);
		}
		else
		{
			break;
		}
	}

	// get substrings
	std::vector<std::string> result;
	std::size_t start = 0;
	std::size_t delimiterSize = delimiter.size();
	for (unsigned i = 0; i < delimiterLocations.size(); i++)
	{
		std::string sub = temp.substr(start, delimiterLocations[i] - start);
		start = delimiterLocations[i] + delimiterSize;
		result.push_back(sub);
	}
	result.push_back(temp.substr(start, std::string::npos)); // add last missing part / or if no delimiter was found add the whole string -> no delimiter found -> one of the original files -> tornade.obj, ring.obj
	return result;
}

std::string Scene::JoinVector(const std::vector<std::string>& vec, const std::string& delimiter)
{
	std::string result;
	for (unsigned i = 0; i < vec.size() - 1; i++)
	{
		result += vec[i] + delimiter;
	}
	result += vec[vec.size() - 1];
	return result;
}


DataDescription& Scene::GetFirstMatchingDatasets(std::string& name)
{
	for (unsigned i = 0; i < mDatasets.size(); i++)
	{
		if (mDatasets[i].name == name)
		{
			return mDatasets[i];
		}
	}
	return DataDescription();
}

DataDescription& Scene::GetFirstMatchingDatasets(std::string& name, std::string& importance)
{
	for (unsigned i = 0; i < mDatasets.size(); i++)
	{
		if (mDatasets[i].name == name && mDatasets[i].importance == importance)
		{
			return mDatasets[i];
		}
	}
	return DataDescription();
}

DistanceMatrixDescription Scene::GetFirstMatchingDistanceMatrix(std::string& name)
{
	for (unsigned i = 0; i < mDistanceMatrices.size(); i++)
	{
		if (mDistanceMatrices[i].name == name)
		{
			return mDistanceMatrices[i];
		}
	}
	return DistanceMatrixDescription();
}

// generalize these two maybe
void Scene::RefreshCurrentDatasetImportance()
{
	mCurrentImportances.clear();
	for (unsigned i = 0; i < mDatasets.size(); i++)
	{
		if (mDatasets[i].name == mCurrentDataset.name)
		{
			mCurrentImportances.insert(mDatasets[i].importance);
		}
	}
}

void Scene::RefreshCurrentDatasetScalarColor()
{
	mCurrentScalarColors.clear();
	for (unsigned i = 0; i < mDatasets.size(); i++)
	{
		if (mDatasets[i].name == mCurrentDataset.name)
		{
			mCurrentScalarColors.insert(mDatasets[i].scalarColor);
		}
	}
}

void Scene::RefreshCurrentDistanceMatrixMetrics()
{
	mDistanceMetrics.clear();
	for (unsigned i = 0; i < mDistanceMatrices.size(); i++)
	{
		if (mDistanceMatrices[i].name == mCurrentDistanceMatrix.name)
		{
			mDistanceMetrics.push_back(mDistanceMatrices[i].metric);
		}
	}
}

std::string Scene::ConstructDatasetPath(DataDescription& data)
{
	std::string path = mDatasetPath + "\\" + data.name;
	
	if (data.importance != "Unknown")
	{
		path += mSeperator + data.importance;
	}
	if (data.scalarColor != "Unknown")
	{
		path += mSeperator + data.scalarColor;
	}
	path += ".obj";

	return path;
}

std::string Scene::ConstructDistanceMatrixPath(DistanceMatrixDescription& data)
{
	std::string path = mDistanceMatrixPath + "\\" + data.name;
	if (data.metric != "Unknown")
	{
		path += mSeperator + data.metric;
	}
	path += ".dist";
	return path;
}
