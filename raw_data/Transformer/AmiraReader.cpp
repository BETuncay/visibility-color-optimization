#include "AmiraReader.hpp"
#include <fstream>
#include <vtkImageData.h>
#include <vtkPointData.h>
#include <vtkFloatArray.h>

namespace vispro
{
	static const char* FindAndJump(const char* buffer, const char* SearchString) {
		const char* FoundLoc = strstr(buffer, SearchString);
		if (FoundLoc) return FoundLoc + strlen(SearchString);
		return buffer;
	}


	vtkSmartPointer<vtkImageData> AmiraReader::ReadField(const char* path, const char* fieldName, Eigen::AlignedBox3d& bounds, Eigen::Vector3i& resolution, Eigen::Vector3d& spacing, int& numComponents)
	{
		// read the header
		if (!ReadHeader(path, bounds, resolution, spacing, numComponents)) return nullptr;

		// allocate output field
		vtkNew<vtkImageData> field;
		field->SetDimensions(resolution.data());
		field->SetOrigin(bounds.min().data());
		field->SetSpacing(spacing.data());
		vtkNew<vtkFloatArray> mArray;
		int64_t numPoints = (int64_t)field->GetDimensions()[0] * field->GetDimensions()[1] * field->GetDimensions()[2];
		mArray->SetNumberOfComponents(numComponents);
		mArray->SetNumberOfTuples(numPoints);
		mArray->SetName(fieldName);
		field->GetPointData()->AddArray(mArray);
		field->GetPointData()->SetActiveScalars(fieldName);

		// read the data
		if (!ReadField(path, mArray)) return nullptr;
		return field;
	}

	vtkSmartPointer<vtkImageData> AmiraReader::ReadField(const char* path, const char* fieldName)
	{
		// read the header
		Eigen::AlignedBox3d bounds;
		Eigen::Vector3i resolution;
		Eigen::Vector3d spacing;
		int numComponents;
		if (!ReadHeader(path, bounds, resolution, spacing, numComponents)) return nullptr;
		
		// allocate output field
		vtkNew<vtkImageData> field;
		field->SetDimensions(resolution.data());
		field->SetOrigin(bounds.min().data());
		field->SetSpacing(spacing.data());
		vtkNew<vtkFloatArray> mArray;
		int64_t numPoints = (int64_t)field->GetDimensions()[0] * field->GetDimensions()[1] * field->GetDimensions()[2];
		mArray->SetNumberOfComponents(numComponents);
		mArray->SetNumberOfTuples(numPoints);
		mArray->SetName(fieldName);
		field->GetPointData()->AddArray(mArray);
		field->GetPointData()->SetActiveScalars(fieldName);

		// read the data
		if (!ReadField(path, mArray)) return nullptr;
		return field;
	}

	bool AmiraReader::ReadHeader(const char* path, Eigen::AlignedBox3d& bounds, Eigen::Vector3i& resolution, Eigen::Vector3d& spacing, int& numComponents)
	{
		// get the path parameter
		FILE* fp = fopen(path, "rb");
		if (!fp) return false;

		// We read the first 2k bytes into memory to parse the header.
		// The fixed buffer size looks a bit like a hack, and it is one, but it gets the job done.
		char buffer[2048];
		fread(buffer, sizeof(char), 2047, fp);
		buffer[2047] = '\0'; //The following string routines prefer null-terminated strings

		if (!strstr(buffer, "# AmiraMesh BINARY-LITTLE-ENDIAN 2.1")) {
			fclose(fp);
			return false;
		}

		// Find the Lattice definition, i.e., the dimensions of the uniform grid
		int xDim(0), yDim(0), zDim(0);
		if (sscanf(FindAndJump(buffer, "define Lattice"), "%d %d %d", &xDim, &yDim, &zDim) == 3)
			resolution = Eigen::Vector3i(xDim, yDim, zDim);

		//Is it a uniform grid? We need this only for the sanity check below.
		const bool bIsUniform = (strstr(buffer, "CoordType \"uniform\"") != NULL);

		// Find the BoundingBox
		float xmin(1.0f), ymin(1.0f), zmin(1.0f);
		float xmax(-1.0f), ymax(-1.0f), zmax(-1.0f);
		if (sscanf(FindAndJump(buffer, "BoundingBox"), "%g %g %g %g %g %g", &xmin, &xmax, &ymin, &ymax, &zmin, &zmax) == 6)
			bounds = Eigen::AlignedBox3d(Eigen::Vector3d(xmin, ymin, zmin), Eigen::Vector3d(xmax, ymax, zmax));

		//Type of the field: scalar, vector
		numComponents = 0;
		if (strstr(buffer, "Lattice { float Data }"))
		{
			// Scalar field
			numComponents = 1;
		}
		else
		{
			// A field with more than one component, i.e., a vector field
			if (sscanf(FindAndJump(buffer, "Lattice { float["), "%d", &numComponents) != 1)
			{
				fclose(fp);
				return false;
			}
		}

		// Sanity check
		if (xDim <= 0 || yDim <= 0 || zDim <= 0 || xmin > xmax || ymin > ymax || zmin > zmax || !bIsUniform || numComponents <= 0) {
			fclose(fp);
			return false;
		}

		// compute spacing
		spacing = Eigen::Vector3d(
			(bounds.max()[0] - bounds.min()[0]) / (resolution[0] - 1.),
			(bounds.max()[1] - bounds.min()[1]) / (resolution[1] - 1.),
			(bounds.max()[2] - bounds.min()[2]) / (resolution[2] - 1.));
		return true;
	}

	bool AmiraReader::ReadField(const char* path, vtkFloatArray* output)
	{
		// get the path parameter
		FILE* fp = fopen(path, "rb");
		if (!fp) return false;

		// We read the first 2k bytes into memory to parse the header.
		// The fixed buffer size looks a bit like a hack, and it is one, but it gets the job done.
		char buffer[2048];
		fread(buffer, sizeof(char), 2047, fp);
		buffer[2047] = '\0'; //The following string routines prefer null-terminated strings

		// Find the beginning of the data section
		const long idxStartData = (long)(strstr(buffer, "# Data section follows") - buffer);
		if (idxStartData > 0)
		{
			// Set the file pointer to the beginning of "# Data section follows"
			fseek(fp, idxStartData, SEEK_SET);
			// Consume this line, which is "# Data section follows"
			fgets(buffer, 2047, fp);
			// Consume the next line, which is "@1"
			fgets(buffer, 2047, fp);

			// read the data
			const size_t numValues = output->GetNumberOfValues();
			const size_t actRead = fread((void*)output->GetPointer(0), sizeof(float), numValues, fp);
			if (numValues != actRead) {
				fclose(fp);
				return false;
			}
			fclose(fp);
			return true;
		}
		fclose(fp);
		return false;
	}
}