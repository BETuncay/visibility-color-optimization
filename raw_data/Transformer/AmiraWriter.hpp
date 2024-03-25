#pragma once

class vtkImageData;

namespace vispro
{
	// Helper class for writing vtkImageData to the Amira format (*.am)
	class AmiraWriter
	{
	public:
		// Writes a scalar field in vtkImageData to file.
		static void WriteScalarField(const char* path, const char* fieldName, vtkImageData* imageData);

		// Writes a vector field in vtkImageData to file.
		static void WriteVectorField(const char* path, const char* fieldUName, const char* fieldVName, const char* fieldWName, vtkImageData* imageData);
	};
}