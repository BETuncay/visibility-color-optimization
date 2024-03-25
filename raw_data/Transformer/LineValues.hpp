#pragma once

#include <vector>
#include <Eigen/Eigen>
#include <vtkSmartPointer.h>
#include <string>
#include "SteadyTracer.hpp"
#include <vtkPointData.h>
#include <vtkImageData.h>
#include <vtkFloatArray.h>
#include "AmiraReader.hpp"
#include "Sampling.hpp"
#include "LineValues.hpp"
#include "Vorticity.hpp"
#include "Acceleration.hpp"

class vtkImageData;
class vtkFloatArray;

typedef std::vector<Eigen::Vector3d> Line;

// importance and scalarColor values
enum LineProperty {
	lineLength,
	curvature,
	velocity,
	vorticity,
};

static const std::vector<LineProperty> AllProperties{
	lineLength,
	curvature,
	velocity,
	vorticity
};

struct LineValue {
	std::vector<std::vector<double>> values;
	double maxValue;
	double minValue;
	LineValue(std::vector<std::vector<double>> values, double maxValue, double minValue) : values(values), maxValue(maxValue), minValue(minValue) {}
	LineValue() = default;
};

namespace vispro
{
	class LineProperties
	{
	public:
		static LineValue CalculateLineProperty(const LineProperty property, const std::vector<Line>& lines, const vtkSmartPointer<vtkImageData>& velocityField, const float stepSize);
		static LineValue CalculateLineLength(const std::vector<Line>& lines);
		static LineValue CalculateCurvature(const std::vector<Line>& lines, const vtkSmartPointer<vtkImageData>& velocityField, const float stepSize);
		static LineValue CalculateVelocity(const std::vector<Line>& lines, const vtkSmartPointer<vtkImageData>& velocityField);
		static LineValue CalculateVorticity(const std::vector<Line>& lines, const vtkSmartPointer<vtkImageData>& velocityField);
		static void NormalizeValues(LineValue& lineValue);
		static void RobustScalar(LineValue& lineValue);
		static void ClampValues(LineValue& lineValue);
		static void SmoothValues(LineValue& lineValue, const double weight, const unsigned smoothingIterations);
		static std::string ToString(const LineProperty property);

	private:
		static double Quartile(std::vector<double>& allValues, double quartile);
	};
}