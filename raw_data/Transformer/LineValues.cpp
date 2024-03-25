#include "LineValues.hpp"

namespace vispro
{

	LineValue LineProperties::CalculateLineProperty(const LineProperty property, const std::vector<Line>& lines, const vtkSmartPointer<vtkImageData>& velocityField = NULL, const float stepSize = 0.f)
	{
		switch (property)
		{
		case lineLength:
			return LineProperties::CalculateLineLength(lines);
		case curvature:
			return LineProperties::CalculateCurvature(lines, velocityField, stepSize);
		case velocity:
			return LineProperties::CalculateVelocity(lines, velocityField);
		case vorticity:
			return LineProperties::CalculateVorticity(lines, velocityField);
		default:
			return LineValue ({}, 0.f, 0.f);
		}
	}

	std::string LineProperties::ToString(const LineProperty property)
	{
		switch (property)
		{
		case lineLength:
			return "lineLength";
		case curvature:
			return "curvature";
		case velocity:
			return "velocity";
		case vorticity:
			return "vorticity";
		default:
			return "";
		}
	}

	LineValue LineProperties::CalculateLineLength(const std::vector<Line>& lines)
	{
		double maxValue = -DBL_MAX;
		double minValue = DBL_MAX;
		std::vector<std::vector<double>> values(lines.size());
		#ifndef _DEBUG
		#pragma omp parallel for
		#endif
		for (long id = 0; id < lines.size(); id++)
		{
			double totalLineLength = 0.f;
			for (unsigned point = 1; point < lines[id].size(); point++)
			{
				Eigen::Vector3d temp = lines[id][point] - lines[id][point - 1];
				totalLineLength += temp.norm();
			}
			maxValue = std::max(maxValue, totalLineLength);
			minValue = std::min(minValue, totalLineLength);
			values[id] = std::vector<double>(lines[id].size(), totalLineLength);
		}
		return LineValue(values, maxValue, minValue);
	}
	
	// vector field = first derivative
	// acceleration field = second derivative
	LineValue LineProperties::CalculateCurvature(const std::vector<Line>& lines, const vtkSmartPointer<vtkImageData>& velocityField, const float stepSize)
	{
		const vtkSmartPointer<vtkImageData>& accelerationField = Acceleration::Compute(velocityField);
		double maxValue = -DBL_MAX;
		double minValue = DBL_MAX;
		std::vector<std::vector<double>> values(lines.size());
		#ifndef _DEBUG
		#pragma omp parallel for
		#endif
		for (long id = 0; id < lines.size(); id++)
		{
			std::vector<double> lineValue(lines[id].size());
			for (unsigned point = 0; point < lines[id].size(); point++)
			{
				Eigen::Vector3d firstDerivative = Sampling::LinearSample3(lines[id][point], velocityField);
				Eigen::Vector3d secondDerivative = Sampling::LinearSample3(lines[id][point], accelerationField);

				double numerator = (firstDerivative.cross(secondDerivative)).norm();
				double denominator = pow(firstDerivative.norm(), 3.0);
				double curvature = 0;
				if (denominator != 0)
				{
					curvature = numerator / denominator;
				}
			
				lineValue[point] = curvature;
				maxValue = std::max(maxValue, curvature);
				minValue = std::min(minValue, curvature);
			}
			values[id] = lineValue;
		}
		return LineValue(values, maxValue, minValue);
	}

	LineValue LineProperties::CalculateVelocity(const std::vector<Line>& lines, const vtkSmartPointer<vtkImageData>& velocityField)
	{
		double maxVelocity = -DBL_MAX;
		double minVelocity = DBL_MAX;
		std::vector<std::vector<double>> velocity(lines.size());
		#ifndef _DEBUG
		#pragma omp parallel for
		#endif
		for (long id = 0; id < lines.size(); id++)
		{
			std::vector<double> lineVelocity(lines[id].size());
			for (unsigned point = 0; point < lines[id].size(); point++)
			{
				Eigen::Vector3d sample = Sampling::LinearSample3(lines[id][point], velocityField.GetPointer());
				double magnitute = sample.norm();
				maxVelocity = std::max(maxVelocity, magnitute);
				minVelocity = std::min(minVelocity, magnitute);
				lineVelocity[point] = magnitute;
			}
			velocity[id] = lineVelocity;
		}
		return LineValue(velocity, maxVelocity, minVelocity);
	}

	LineValue LineProperties::CalculateVorticity(const std::vector<Line>& lines, const vtkSmartPointer<vtkImageData>& velocityField)
	{
		vtkSmartPointer<vtkImageData> vorticityField = Vorticity::Compute(velocityField);

		double maxValue = -DBL_MAX;
		double minValue = DBL_MAX;
		std::vector<std::vector<double>> values(lines.size());
		#ifndef _DEBUG
		#pragma omp parallel for
		#endif
		for (long id = 0; id < lines.size(); id++)
		{
			std::vector<double> lineValue(lines[id].size());
			for (unsigned point = 0; point < lines[id].size(); point++)
			{
				double sample = Sampling::LinearSample1(lines[id][point], vorticityField.GetPointer());
				maxValue = std::max(maxValue, sample);
				minValue = std::min(minValue, sample);
				lineValue[point] = sample;
			}
			values[id] = lineValue;
		}
		return LineValue(values, maxValue, minValue);
	}
	
	void LineProperties::NormalizeValues(LineValue& lineValue)
	{
		double diff = lineValue.maxValue - lineValue.minValue;
		if (diff == 0)
		{
			diff = 0;
		}
		else
		{
			diff = 1 / diff;
		}

		for (std::vector<double>& values : lineValue.values)
		{
			for (double& value : values)
			{
				value = (value - lineValue.minValue) * diff;
			}
		}
	}

	// calculate q_25, q_75 quantiles
	// calcualte inter_quartile_range: iqr = q_75 - q_25
	// clamp value if smalller than q_25 - iqr * factor
	// clamp value if larger than q_75 + iqr * factor
	void LineProperties::ClampValues(LineValue& lineValue)
	{
		unsigned numPoints = 0;
		for (std::vector<double>& values : lineValue.values)
		{
			numPoints += values.size();
		}

		if (numPoints == 0)
		{
			return;
		}

		unsigned index = 0;
		std::vector<double> allValues(numPoints);
		for (std::vector<double>& values : lineValue.values)
		{
			std::copy(values.begin(), values.end(), allValues.begin() + index);
			index += values.size();
		}
		std::sort(allValues.begin(), allValues.end());


		double firstQuantile = LineProperties::Quartile(allValues, 0.25);
		double thirdQuantile = LineProperties::Quartile(allValues, 0.75);
		double interQuartileRange = thirdQuantile - firstQuantile;
		interQuartileRange *= 1.5;
		double upper = thirdQuantile + interQuartileRange;
		double lower = firstQuantile - interQuartileRange;

		unsigned clampCount = 0;
		lineValue.maxValue = -DBL_MAX;
		lineValue.minValue = DBL_MAX;
		for (std::vector<double>& values : lineValue.values)
		{
			for (double& value : values)
			{
				if (value < lower)
				{
					float noise = (rand() / (float)(RAND_MAX)) * interQuartileRange * 0.1;
					value = lower;
					clampCount++;
				}
				if (value > upper)
				{
					float noise = (rand() / (float)(RAND_MAX)) * interQuartileRange * 0.1;
					value = upper;
					clampCount++;
				}
				lineValue.maxValue = std::max(value, lineValue.maxValue);
				lineValue.minValue = std::min(value, lineValue.minValue);
			}
		}
		std::cout << "Clamped " << clampCount << " of " << numPoints <<" values.\n";
	}
	
	// smooth values using the immediate neighbourhood
	// calculate forward and backward derivative around the value and weight the result
	// v' = v + ((l - v) / 2 + (r - v)/2) * w		// v = value, l = left, r = right, w = weight
	// v' = v + ((l + r) / 2 - v) * w
	// v' = v + (l + r) * w / 2 - v * w
	// v' = v * (1 - w) + (l + r) * w / 2
	// border cases:
	// v' = v + (l - v) * w
	// v' = v + l * w - v * w
	// v' = v * (l - w) + l * w;
	void LineProperties::SmoothValues(LineValue& lineValue, const double weight, const unsigned smoothingIterations)
	{	
		for (unsigned iteration = 0; iteration < smoothingIterations; iteration++)
		{
			for (std::vector<double>& values : lineValue.values)
			{
				if (values.size() > 1)
				{
					std::vector<double> previous = values;	// could solve this more efficiently -> only need to remember small window of numbers
					values[0] = previous[0] + (previous[1] - previous[0]) * weight;
					for (unsigned i = 1; i < values.size() - 1; i++)
					{
						values[i] = previous[i] + ((previous[i - 1] + previous[i + 1]) * 0.5 - previous[i]) * weight;
					}
					values[values.size() - 1] = previous[values.size() - 1] + (previous[values.size() - 2] - previous[values.size() - 1]) * weight;
				}
			}
		}
	}

	// scaled_value = (value - median) / (quantial_75 - quantial_25);
	// first copy all line values into contiguous vector
	// then sort vector and calcualte quantiles
	void LineProperties::RobustScalar(LineValue& lineValue)
	{
		unsigned numPoints = 0;
		for (std::vector<double>& values : lineValue.values)
		{
			numPoints += values.size();
		}

		if (numPoints == 0)
		{
			return;
		}

		unsigned index = 0;
		std::vector<double> allValues(numPoints);
		for (std::vector<double>& values : lineValue.values)
		{
			std::copy(values.begin(), values.end(), allValues.begin() + index);
			index += values.size();
		}
		std::sort(allValues.begin(), allValues.end());
		

		double firstQuantile = LineProperties::Quartile(allValues, 0.25);
		double secondQuantile = LineProperties::Quartile(allValues, 0.5);
		double thirdQuantile = LineProperties::Quartile(allValues, 0.75);
		double interQuartileRange = thirdQuantile - firstQuantile;
		interQuartileRange = 1 / interQuartileRange;
		for (std::vector<double>& values : lineValue.values)
		{
			for (double& value : values)
			{
				value = (value - secondQuantile) * interQuartileRange;
			}
		}
	}

	double LineProperties::Quartile(std::vector<double>& allValues, double quartile)
	{
		double indexDouble = allValues.size() * quartile;
		unsigned index = std::floor(indexDouble);
		if (indexDouble == index)
		{
			return (allValues[index - 1] + allValues[index]) * 0.5;
		}
		else
		{
			return allValues[index];
		}
	}	
}