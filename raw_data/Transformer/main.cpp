#include <iostream>
#include "AmiraReader.hpp"
#include <vtkImageData.h>
#include <vtkSmartPointer.h>
#include "AmiraReader.hpp"
#include "AmiraWriter.hpp"
#include "SteadyTracer.hpp"
#include "ObjectWriter.hpp"
#include "Vorticity.hpp"
#include "Sampling.hpp"
#include "LineValues.hpp"
#include "Normalize.hpp"
#include "ObjectReader.hpp"
#include "LineDistanceMetrics.hpp"

#include "stdafx.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "dataanalysis.h"

using namespace vispro;

struct ExtractConfig {
	std::string name;
	unsigned int numParticles;
	unsigned int numSteps;
	double maxLength;
	double stepSize;
	NormalizationMethod normalization;
	unsigned minPointAmountPerLine;
	double minLineLength;
	double minPointDistance;
	bool clampProperty;
	bool smoothProerty;
};

struct DistanceConfig {
	std::string name;
	DistanceMetrics metric;
	float importanceWeight;
	float scalarColorWeight;
	bool normalization;
};

std::string AmiraPath(const std::string& basePath, const std::string& name)
{
	return basePath + "\\" + name + ".am";
}

std::string ObjectPath(const std::string& basePath, const std::string& name, const std::string& importanceName, const std::string& scalarName)
{
	std::string output = basePath + "\\" + name;
	if (importanceName != "")
	{
		output += "---" + importanceName;
	}
	if (scalarName != "")
	{
		output += "---" + scalarName;
	}
	output += ".obj";
			
	return output;
}

std::string DistanceMatrixPath(const std::string& basePath, const std::string& name, const std::string& metric, const float importanceWeight, const float scalarColorWeight, const bool normalized)
{
	std::string output = basePath + "\\" + name + "---" + metric + "---" + std::to_string(importanceWeight) + "---" + std::to_string(scalarColorWeight);
	if (normalized)
	{
		output += "---Normalized";
	}
	output += ".dist";
	return output;
}

void saveDistanceMatrix(const std::string& filename, alglib::real_2d_array& d)
{
	std::ofstream out(filename, std::ofstream::out);
	out << d.tostring(6);
	out.close();
}

void ExtractSingleLineData(const ExtractConfig& config, std::string& inputPath, std::string& outputPath, LineProperty importance, LineProperty scalarColor)
{
	// get input and output locations
	std::string input = AmiraPath(inputPath, config.name);
	std::string output = ObjectPath(outputPath, config.name, LineProperties::ToString(importance), LineProperties::ToString(scalarColor));

	// load header/data
	Eigen::AlignedBox3d bounds;
	Eigen::Vector3i resolution;
	Eigen::Vector3d spacing;
	int numComponents;
	vtkSmartPointer<vtkImageData> velocityField = AmiraReader::ReadField(input.c_str(), "velocity", bounds, resolution, spacing, numComponents);

	double normalizationFactor = 1.0;
	if (config.normalization == NormalizationMethod::MinMax)
	{
		normalizationFactor = Normalize::InverseMaximumMagnitude(velocityField);
	}

	// calculate lines
	std::vector<Line> lines;
	SteadyTracer::Streamline(lines, velocityField, bounds, config.numParticles, config.numSteps, config.maxLength, config.stepSize, normalizationFactor);
	SteadyTracer::FilterLinesSize(lines, 3);

	// get importance value for each line vertex
	LineValue lineImportance = LineProperties::CalculateLineProperty(importance, lines, velocityField, config.stepSize);
	LineProperties::NormalizeValues(lineImportance);

	// get scalarColor for each line vertex
	LineValue lineScalarColor = LineProperties::CalculateLineProperty(scalarColor, lines, velocityField, config.stepSize);
	LineProperties::NormalizeValues(lineScalarColor);

	// convert lines to obj file
	ObjectWriter::WriteObject(output.c_str(), lines, lineImportance.values, lineScalarColor.values);
}

double pickMedian(std::vector<double>& sortedValues)
{
	if (sortedValues.size() & 2 == 0)
	{
		unsigned mid = sortedValues.size() / 2;
		return 0.5 * (sortedValues[mid] + sortedValues[mid - 1]);
	}
	else
	{
		unsigned mid = (sortedValues.size() - 1) / 2;
		return sortedValues[mid];
	}
}

int pickMedian(std::vector<int>& sortedValues)
{
	if (sortedValues.size() & 2 == 0)
	{
		unsigned mid = sortedValues.size() / 2;
		return 0.5 * (sortedValues[mid] + sortedValues[mid - 1]);
	}
	else
	{
		unsigned mid = (sortedValues.size() - 1) / 2;
		return sortedValues[mid];
	}
}

void PrintLineProperties(std::vector<Line>& lines)
{
	if (lines.size() == 0)
	{
		std::cout << "All lines have been filterd \n";
		return;
	}

	std::vector<double> lineLengths(lines.size(), 0.0);
	std::vector<int> lineCounts(lines.size(), 0);
	std::vector<double> pointDistances;

	long totalNumPoints = 0;
	// calculate all line lengths / sizes
	for (unsigned line = 0; line < lines.size(); line++)
	{
		lineCounts[line] = lines[line].size();
		totalNumPoints += lines[line].size();
		for (unsigned i = 1; i < lines[line].size(); i++)
		{
			double distance = (lines[line][i] - lines[line][i - 1]).norm();
			lineLengths[line] += distance;
			pointDistances.push_back(distance);
		}
	}

	// calcualte max length / size
	double maxLineLength = -DBL_MAX;
	int maxCount = 0;
	for (int i = 0; i < lineLengths.size(); i++)
	{
		maxLineLength = std::max(lineLengths[i], maxLineLength);
		maxCount = std::max(lineCounts[i], maxCount);
	}

	// calculate mean length / size
	double meanLength = 0.0;
	double meanCount = 0.0;
	for (int i = 0; i < lineLengths.size(); i++)
	{
		meanLength += lineLengths[i];
		meanCount += lineCounts[i];
	}
	meanLength /= lines.size();
	meanCount /= lines.size();

	// calculate std length / size
	double stdLength = 0.0;
	double stdCount = 0.0;
	for (int i = 0; i < lineLengths.size(); i++)
	{
		stdLength += pow((lineLengths[i] - meanLength), 2.0);
		stdCount += pow((lineCounts[i] - meanCount), 2.0);
	}
	stdLength = sqrt(stdLength / lines.size());
	stdCount = sqrt(stdCount / lines.size());

	// calculate median
	std::sort(std::begin(lineLengths), std::end(lineLengths));
	std::sort(std::begin(lineCounts), std::end(lineCounts));
	double medianLength = pickMedian(lineLengths);
	int medianCount = pickMedian(lineCounts);
	

	// calculate mad (median absolute deviation)
	for (unsigned i = 0; i < lineLengths.size(); i++)
	{
		lineLengths[i] = abs(lineLengths[i] - medianLength);
		lineCounts[i] =	abs(lineCounts[i] - medianCount);
	}
	std::sort(std::begin(lineLengths), std::end(lineLengths));
	std::sort(std::begin(lineCounts), std::end(lineCounts));
	double madLength = pickMedian(lineLengths);
	int madCount = pickMedian(lineCounts);
	
	// calculate median and mad for point distances
	// median
	std::sort(std::begin(pointDistances), std::end(pointDistances));
	double medianPointDistance = pickMedian(pointDistances);

	// calculate mad (median absolute deviation)
	for (unsigned i = 0; i < pointDistances.size(); i++)
	{
		pointDistances[i] = abs(pointDistances[i] - medianPointDistance);
	}
	double madPointDistance = pickMedian(pointDistances);

	std::cout << "Max Length/Count: " << maxLineLength << ", " << maxCount << "\n";
	//std::cout << "Mean/Std Length: " << meanLength << ", " << stdLength << "\n";
	std::cout << "Median/Mad Length: " << medianLength << ", " << madLength << "\n";
	//std::cout << "Mean/Std Count: " << meanCount << ", " << stdCount << "\n";
	std::cout << "Median/Mad Count: " << medianCount << ", " << madCount << "\n";
	std::cout << "Median/Mad Point Distances" << medianPointDistance << ", " << madPointDistance << "\n";
	std::cout << "Lines: " << lines.size() << "\n";
	std::cout << "Total num points: " << totalNumPoints << "\n\n";
}

// extract all possible combinations of properties
void ExtractAllLineData(const ExtractConfig& config, std::string& inputPath, std::string& outputPath)
{
	// get input and output locations
	std::string input = AmiraPath(inputPath, config.name);

	// load header/data
	Eigen::AlignedBox3d bounds;
	Eigen::Vector3i resolution;
	Eigen::Vector3d spacing;
	int numComponents;
	vtkSmartPointer<vtkImageData> velocityField = AmiraReader::ReadField(input.c_str(), "velocity", bounds, resolution, spacing, numComponents);

	// normalize vector field
	double normalizationFactor = 1.0;
	if (config.normalization == NormalizationMethod::MinMax)
	{
		normalizationFactor = Normalize::InverseMaximumMagnitude(velocityField);
	}

	// calculate lines
	std::vector<Line> lines;
	SteadyTracer::Streamline(lines, velocityField, bounds, config.numParticles, config.numSteps, config.maxLength, config.stepSize, normalizationFactor);
	
	std::cout << "Properties before filtering:\n";
	PrintLineProperties(lines);

	SteadyTracer::FilterLinesSize(lines, config.minPointAmountPerLine);
	SteadyTracer::FilterLinesLength(lines, config.minLineLength);
	SteadyTracer::FilterIntraLineDistance(lines, config.minPointDistance);
	
	std::cout << "Properties after filtering:\n";
	PrintLineProperties(lines);

	// calculate line properties
	std::unordered_map<LineProperty, LineValue> lineValues;
	for (LineProperty prop : AllProperties)
	{
		LineValue lineValue = LineProperties::CalculateLineProperty(prop, lines, velocityField, config.stepSize);
		for (auto& line : lineValue.values)
		{
			for (auto& value : line)
			{
				if (std::isnan(value))
				{
					std::cout << value << "nan value found in property"<< LineProperties::ToString(prop) << "\n";
				}
			}
		}
		if (config.clampProperty)
		{
			LineProperties::ClampValues(lineValue);
		}
		LineProperties::NormalizeValues(lineValue);
		if (config.clampProperty)
		{
			LineProperties::SmoothValues(lineValue, 0.05, 10);
		} 
		lineValues.insert({ prop, lineValue });
	}

	// save results
	for (LineProperty importance : AllProperties)
	{
		for (LineProperty scalarColor : AllProperties)
		{
			std::string output = ObjectPath(outputPath, config.name, LineProperties::ToString(importance), LineProperties::ToString(scalarColor));
			ObjectWriter::WriteObjectFast(output.c_str(), lines, lineValues[importance].values, lineValues[scalarColor].values);
		}
	}
}

// Get Bounding Box and return dimension with largest interval
Range GetMinMaxValues(const Lines5d& lines)
{
	Range xRange;
	Range yRange;
	Range zRange;
	for (const Line5d& line : lines)
	{
		for (unsigned i = 0; i < line.size(); i++)
		{
			Eigen::Vector3d vertex;
			vertex[1] = line[i][1];
			vertex[2] = line[i][2];

			xRange.max = std::max(line[i][0], xRange.max);
			xRange.min = std::min(line[i][0], xRange.min);
			yRange.max = std::max(line[i][1], yRange.max);
			yRange.min = std::min(line[i][1], yRange.min);
			zRange.max = std::max(line[i][2], zRange.max);
			zRange.min = std::min(line[i][2], zRange.min);
		}
	}

	double xDiff = xRange.max - xRange.min;
	double yDiff = yRange.max - yRange.min;
	double zDiff = zRange.max - zRange.min;

	if (xDiff >= yDiff && xDiff >= zDiff)
	{
		return xRange;
	}

	if (yDiff >= xDiff && yDiff >= zDiff)
	{
		return yRange;
	}
	return zRange;
}

// scales values from [0, 1] -> [range.min, range.max]
void ScaleValues(Lines5d& lines, unsigned index, const Range& range)
{
	for (Line5d& line : lines)
	{
		for (Eigen::Vector<double, 5>& value : line)
		{
			value[index] = value[index] * (range.max - range.min) + range.min;
		}
	}
}

void ExtractAllDistaneMetrics(const DistanceConfig& config, const std::string& inputPath, const std::string& outputPath, const std::string& importance = "", const std::string& scalarColor = "")
{
	std::string in = ObjectPath(inputPath, config.name, importance, scalarColor);
	Lines5d lines = ObjectReader::ReadObject(in);

	if (config.normalization)
	{
		Range range = GetMinMaxValues(lines);
		ScaleValues(lines, 3, range);
		ScaleValues(lines, 4, range);
	}

	// weight importance / scalarColor dimension
	Range importanceWeight = { 0.0, config.importanceWeight };
	Range scalarColoreWeight = { 0.0, config.scalarColorWeight };
	ScaleValues(lines, 3, importanceWeight);
	ScaleValues(lines, 4, scalarColoreWeight);

	Eigen::MatrixXd distance = LineDistanceMetrics::Compute_Metric(lines, config.metric);
	alglib::real_2d_array d;
	d.attach_to_ptr(lines.size(), lines.size(), distance.data());

	std::string out = DistanceMatrixPath(outputPath, config.name, LineDistanceMetrics::ToString(config.metric), config.importanceWeight, config.scalarColorWeight, config.normalization);
	saveDistanceMatrix(out, d);
}

int main(int, char*[])
{
	srand((unsigned int)time(0));
	std::string amiraPath	= "raw_data";
	std::string objectPath	= "B:\\_Repos\\copy\\visibility-color-optimization\\data";
	std::string distPath	= "B:\\_Repos\\copy\\visibility-color-optimization\\distanceMatrix";

	// ---------------------------------------------------------------------------------------------------------------------------
	// streamline extraction -----------------------------------------------------------------------------------------------------
	// ---------------------------------------------------------------------------------------------------------------------------
	std::vector<ExtractConfig> Configurations = 
	{
		// name,							numPartics,	numSteps,	maxLength,	stepSize,	normalization,			minPointAmount, minLineLength, minPointDistance, clampProperty, smoothProperty
		//{"benzene",						9000000,	5000,		900.0,		10.0,		NormalizationMethod::MinMax,	10,		0.0,	0.01,	false,	false},
		//{"benzene",						500000,		50000,		50.0,		0.01,		NormalizationMethod::MinMax,	10,		1.0,	0.01,	false,	false},
		//{"borromean",						2000,		10000,		100.0,		0.1,		NormalizationMethod::MinMax,	10,		4.0,	0.05,	true,	true},
		//{"ECMWF_3D_Reanalysis_Velocity_T0", 5000,		40000,		1000.0,		1.0,		NormalizationMethod::MinMax,	10,		30.0,	0.2,	false,	false},
		//{"ECMWF_3D_Reanalysis_Velocity_T0", 1500,		40000,		1000.0,		1.0,		NormalizationMethod::MinMax,	10,		30.0,	0.2,	true,	true},
		//{"trefoil10",						5000,		5000,		100.0,		0.01,		NormalizationMethod::MinMax,	10,		2.0,	0.01,	true,	true},
		//{"trefoil140",					8000,		2000,		100.0,		0.1,		NormalizationMethod::MinMax,	10,		2.0,	0.02,	false,	false},
		//{"UCLA_CTBL_Velocity_T6",			1500,		40000,		1000,		1.0,		NormalizationMethod::MinMax,	10,		2.0,	0.2,	false,	false},
		{"delta65_high",					8000,		50000,		200.0,		0.001f,		NormalizationMethod::MinMax,	10,		2.0,	0.05,	false,	false},
	};


	// extract streamlines + extract all scalar values + store to disc
#if 1
	for (ExtractConfig& config : Configurations)
	{
		std::cout << "Extracting streamlines from: " << config.name << '\n';
		ExtractAllLineData(config, amiraPath, objectPath);
		std::cout << "Finished streamline extraction from: " << config.name << "\n\n";
	}
#endif


	// ---------------------------------------------------------------------------------------------------------------------------
	// distance matrix extraction ------------------------------------------------------------------------------------------------
	// ---------------------------------------------------------------------------------------------------------------------------
	std::vector<DistanceConfig> DistanceConfigurations = 
	{
		{"borromean", DistanceMetrics::rMCPD, 0.0, 0.0, true},
		/*
		{"borromean", DistanceMetrics::rMCPD, 0.0, 0.0, true},
		{"borromean", DistanceMetrics::rMCPD, 0.5, 0.0, true},
		{"borromean", DistanceMetrics::rMCPD, 2.0, 0.0, true},

		{"borromean", DistanceMetrics::rMCPD, 0.0, 0.5, true},
		{"borromean", DistanceMetrics::rMCPD, 0.5, 0.5, true},
		{"borromean", DistanceMetrics::rMCPD, 2.0, 0.5, true},

		{"borromean", DistanceMetrics::rMCPD, 0.0, 2.0, true},
		{"borromean", DistanceMetrics::rMCPD, 0.5, 2.0, true},
		{"borromean", DistanceMetrics::rMCPD, 2.0, 2.0, true},
		*/

		//{"rings", DistanceMetrics::rMCPD,	0.0, 0.0, true},
		//{"tornado", DistanceMetrics::rMCPD, 0.0, 0.0, true},
		//{"heli", DistanceMetrics::rMCPD, 0.0, 0.0, true},
		
		//{"borromean", DistanceMetrics::rMCPD, 1.0, 0.0, true},
		//{"borromean", DistanceMetrics::MeanL2, 0.0, 0.0, true},
		
		//{"benzene", DistanceMetrics::rMCPD, 1.0, 0.0, true},
		//{"benzene", DistanceMetrics::MeanL2, 0.0, 0.0, true},
		
		//{"ECMWF_3D_Reanalysis_Velocity_T0", DistanceMetrics::rMCPD, 1.0, 0.0, true},
		//{"ECMWF_3D_Reanalysis_Velocity_T0", DistanceMetrics::MeanL2, 0.0, 0.0, true},
		
		//{"trefoil10", DistanceMetrics::rMCPD, 1.0, 0.0, true},
		//{"trefoil10", DistanceMetrics::MeanL2, 0.0, 0.0, true},
		
		//{"trefoil140", DistanceMetrics::rMCPD,  1.0, 0.0, true},
		//{"trefoil140", DistanceMetrics::MeanL2, 0.0, 0.0, true},
		
		//{"UCLA_CTBL_Velocity_T6", DistanceMetrics::rMCPD,  1.0, 0.0, true},
		//{"UCLA_CTBL_Velocity_T6", DistanceMetrics::MeanL2, 0.0, 0.0, true},

		{"delta65_high", DistanceMetrics::rMCPD, 0.0, 0.0, true},
		//{"delta65_high", DistanceMetrics::rMCPD, 1.0, 0.0, true},
	};


	// compute distance matrix of specified datasets
	// need to specify which importance / scalar color values are used
	// if unknown -> leave out those parameters (e.g. for datasets like tornado without amira file)
#if 1
	for (auto config : DistanceConfigurations)
	{
		std::cout << "Calculating distance matrix of : " << config.name << '\n';
		//ExtractAllDistaneMetrics(config, objectPath, distPath);
		ExtractAllDistaneMetrics(config, objectPath, distPath, LineProperties::ToString(LineProperty::lineLength), LineProperties::ToString(LineProperty::vorticity));
		std::cout << "Finished calculations of: " << config.name << "\n\n";
	}
#endif


	// ---------------------------------------------------------------------------------------------------------------------------
	// print line data -----------------------------------------------------------------------------------------------------------
	// ---------------------------------------------------------------------------------------------------------------------------
	// print some stats about the line data stored in the obj file
	// reuseing the distance configuration
#if 0
	for (auto config : DistanceConfigurations)
	{
		std::cout << "Print line metrics of : " << config.name << '\n';
		//std::string in = ObjectPath(objectPath, config.name, "", "");
		std::string in = ObjectPath(objectPath, config.name, LineProperties::ToString(LineProperty::lineLength), LineProperties::ToString(LineProperty::vorticity));
		
		Lines5d lines5d = ObjectReader::ReadObject(in);

		std::vector<Line> lines(lines5d.size());
		for (int i = 0; i < lines5d.size(); i++)
		{
			lines[i].resize(lines5d[i].size());
			for (int j = 0; j < lines5d[i].size(); j++)
			{
				lines[i][j] = Eigen::Vector3d(lines5d[i][j][0], lines5d[i][j][1], lines5d[i][j][2]);
			}
		}
		PrintLineProperties(lines);
	}
#endif


	// ---------------------------------------------------------------------------------------------------------------------------
	// save importance / scalar color to file-------------------------------------------------------------------------------------
	// ---------------------------------------------------------------------------------------------------------------------------
	// load obj file and store importance / scalar color information to a file
	// reuseing the distance configuration
#if 0
	for (auto config : DistanceConfigurations)
	{
		std::cout << "Save importance and scalar color of line to file: " << config.name << '\n';
		//std::string in = ObjectPath(objectPath, config.name, "", "");
		std::string in = ObjectPath(objectPath, config.name, LineProperties::ToString(LineProperty::lineLength), LineProperties::ToString(LineProperty::vorticity));

		Lines5d lines5d = ObjectReader::ReadObject(in);
		
		std::string importance_str = config.name + "_importance.txt";
		std::string scalarColor_str = config.name + "_coloration.txt";
		std::ofstream myfile1(importance_str);
		std::ofstream myfile2(scalarColor_str);
		if (myfile1.is_open() && myfile2.is_open())
		{
			for (Line5d& line : lines5d)
			{
				for (auto& point : line)
				{
					myfile1 << point[3] << " ";
					myfile2 << point[4] << " ";
				}
			}
			myfile1.close();
			myfile2.close();
		}
	}
#endif

	return 0;
}