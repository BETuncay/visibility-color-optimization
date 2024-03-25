#include "AmiraWriter.hpp"
#include <fstream>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkFloatArray.h>

namespace vispro
{
	void AmiraWriter::WriteScalarField(const char* path, const char* fieldName, vtkImageData* imageData)
	{
		int* resolution = imageData->GetDimensions();
		double* spacing = imageData->GetSpacing();
		double* minCorner = imageData->GetOrigin();
		double maxCorner[3] = {
			minCorner[0] + spacing[0] * (resolution[0] - 1),
			minCorner[1] + spacing[1] * (resolution[1] - 1),
			minCorner[2] + spacing[2] * (resolution[2] - 1)
		};

		// Write header
		{
			std::ofstream outStream(path);
			outStream << "# AmiraMesh BINARY-LITTLE-ENDIAN 2.1\n\n\n";
			outStream << "define Lattice " << resolution[0] << " " << resolution[1] << " " << resolution[2] << "\n\n";
			outStream << "Parameters {\n";
			outStream << "Content \"" << resolution[0] << "x" << resolution[1] << "x" << resolution[2] << " float[3], uniform coordinates\",\n";
			outStream << "\tBoundingBox " << minCorner[0] << " " << maxCorner[0] << " " << minCorner[1] << " " << maxCorner[1] << " " << minCorner[2] << " " << maxCorner[2] << ",\n";
			outStream << "\tCoordType \"uniform\"\n";
			outStream << "}\n\n";
			outStream << "Lattice { float Data } @1\n\n";
			outStream << "# Data section follows\n";
			outStream << "@1\n";
			outStream.close();
		}

		// Write data
		{
			std::ofstream outStream(path, std::ios::out | std::ios::app | std::ios::binary);
			vtkFloatArray* floatArray = dynamic_cast<vtkFloatArray*>(imageData->GetPointData()->GetArray(fieldName));
			outStream.write((char*)floatArray->GetPointer(0), sizeof(float) * resolution[0] * resolution[1] * resolution[2]);
			outStream.close();
		}
	}

	void AmiraWriter::WriteVectorField(const char* path, const char* fieldUName, const char* fieldVName, const char* fieldWName, vtkImageData* imageData)
	{
		int* resolution = imageData->GetDimensions();
		double* spacing = imageData->GetSpacing();
		double* minCorner = imageData->GetOrigin();
		double maxCorner[3] = {
			minCorner[0] + spacing[0] * (resolution[0] - 1),
			minCorner[1] + spacing[1] * (resolution[1] - 1),
			minCorner[2] + spacing[2] * (resolution[2] - 1)
		};

		// Write header
		{
			std::ofstream outStream(path);
			outStream << "# AmiraMesh BINARY-LITTLE-ENDIAN 2.1\n\n\n";
			outStream << "define Lattice " << resolution[0] << " " << resolution[1] << " " << resolution[2] << "\n\n";
			outStream << "Parameters {\n";
			outStream << "Content \"" << resolution[0] << "x" << resolution[1] << "x" << resolution[2] << " float[3], uniform coordinates\",\n";
			outStream << "\tBoundingBox " << minCorner[0] << " " << maxCorner[0] << " " << minCorner[1] << " " << maxCorner[1] << " " << minCorner[2] << " " << maxCorner[2] << ",\n";
			outStream << "\tCoordType \"uniform\"\n";
			outStream << "}\n\n";
			outStream << "Lattice { float[3] Data } @1\n\n";
			outStream << "# Data section follows\n";
			outStream << "@1\n";
			outStream.close();
		}

		// Write data
		{
			std::ofstream outStream(path, std::ios::out | std::ios::app | std::ios::binary);
			vtkFloatArray* floatUArray = dynamic_cast<vtkFloatArray*>(imageData->GetPointData()->GetArray(fieldUName));
			vtkFloatArray* floatVArray = dynamic_cast<vtkFloatArray*>(imageData->GetPointData()->GetArray(fieldVName));
			vtkFloatArray* floatWArray = dynamic_cast<vtkFloatArray*>(imageData->GetPointData()->GetArray(fieldWName));
			std::vector<float> interleaved(resolution[0] * resolution[1] * resolution[2] * 3);
#ifndef _DEBUG
#pragma omp parallel
#endif
			for (size_t i = 0; i < interleaved.size() / 3; ++i) {
				interleaved[i * 3 + 0] = floatUArray->GetValue(i);
				interleaved[i * 3 + 1] = floatVArray->GetValue(i);
				interleaved[i * 3 + 2] = floatWArray->GetValue(i);
			}
			outStream.write((char*)interleaved.data(), sizeof(float) * interleaved.size());
			outStream.close();
		}
	}
}