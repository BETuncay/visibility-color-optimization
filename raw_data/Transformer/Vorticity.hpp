#pragma once

#include <vtkImageData.h>
#include <vtkSmartPointer.h>
#include "AmiraReader.hpp"
#include "AmiraWriter.hpp"
#include <vtkPointData.h>
#include <vtkFloatArray.h>

namespace vispro
{
	// Computes the vorticity of a given time step.
	class Vorticity
	{
	public:
		// receives the paths to the vtkImageData file of velocity (input) and the vorticity path (output)
		static void Compute(const char* velocityPath, const char* vorticityPath);
		static vtkSmartPointer<vtkImageData> Compute(const vtkSmartPointer<vtkImageData>& velocityField);
	private:
	};
}