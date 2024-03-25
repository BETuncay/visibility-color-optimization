#pragma once

#include <vtkSmartPointer.h>
#include <Eigen/Eigen>

class vtkImageData;
class vtkFloatArray;

namespace vispro
{
	// Helper class for writing vtkImageData to the Amira format (*.am)
	class AmiraReader
	{
	public:
		// Reads a field into a vtkImageData.
		static vtkSmartPointer<vtkImageData> ReadField(const char* path, const char* fieldName);

		// Reads a field into a vtkImageData and returns header data.
		static vtkSmartPointer<vtkImageData> AmiraReader::ReadField(const char* path, const char* fieldName, Eigen::AlignedBox3d& bounds, Eigen::Vector3i& resolution, Eigen::Vector3d& spacing, int& numComponents);

		// Reads the bounding box and the resolution from an amira file.
		static bool ReadHeader(const char* path, Eigen::AlignedBox3d& bounds, Eigen::Vector3i& resolution, Eigen::Vector3d& spacing, int& numComponents);

		// Reads a field into a pre-allocated vtkFloatArray. Note that it needs to have the right size allocated!
		static bool ReadField(const char* path, vtkFloatArray* output);
	};
}