#pragma once

#include <vtkImageData.h>
#include <vtkSmartPointer.h>
#include "AmiraReader.hpp"
#include "AmiraWriter.hpp"
#include <vtkPointData.h>
#include <vtkFloatArray.h>


enum NormalizationMethod {
	None,
	MinMax,
	//Unit deprecated
};

namespace vispro
{
	class Normalize
	{
	public:
		static vtkSmartPointer<vtkImageData> NormalizeField(const vtkSmartPointer<vtkImageData>& velocityField, const NormalizationMethod method);
		static vtkSmartPointer<vtkImageData> MinMax(const vtkSmartPointer<vtkImageData>& velocityField);
		static vtkSmartPointer<vtkImageData> Unit(const vtkSmartPointer<vtkImageData>& velocityField);
		static double InverseMaximumMagnitude(const vtkSmartPointer<vtkImageData>& velocityField);

	private:
	};
}