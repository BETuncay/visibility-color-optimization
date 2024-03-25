#pragma once

#include <vtkImageData.h>
#include <vtkSmartPointer.h>
#include "AmiraReader.hpp"
#include "AmiraWriter.hpp"
#include <vtkPointData.h>
#include <vtkFloatArray.h>

namespace vispro
{
	class Acceleration
	{
	public:
		static vtkSmartPointer<vtkImageData> Compute(const vtkSmartPointer<vtkImageData>& velocityField);
	private:
	};
}