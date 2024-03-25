#include "Vorticity.hpp"


namespace vispro
{
	void Vorticity::Compute(const char* velocityPath, const char* vorticityPath)
	{
		// read the input file
		vtkSmartPointer<vtkImageData> velocityImage = AmiraReader::ReadField(velocityPath, "velocity");
		vtkFloatArray* velocityArray = dynamic_cast<vtkFloatArray*>(velocityImage->GetPointData()->GetArray("velocity"));

		// allocate output
		vtkNew<vtkImageData> vorticityImage;
		vorticityImage->SetDimensions(velocityImage->GetDimensions());
		vorticityImage->SetOrigin(velocityImage->GetOrigin());
		vorticityImage->SetSpacing(velocityImage->GetSpacing());
		vtkNew<vtkFloatArray> vorticityArray;
		int64_t numPoints = (int64_t)vorticityImage->GetDimensions()[0]
			* vorticityImage->GetDimensions()[1]
			* vorticityImage->GetDimensions()[2];
		vorticityArray->SetNumberOfTuples(numPoints);
		vorticityArray->SetNumberOfComponents(1);
		vorticityArray->SetName("vorticity");
		vorticityImage->GetPointData()->AddArray(vorticityArray);

		// compute the vorticity field
		int* res = vorticityImage->GetDimensions();
		for (int iz = 0; iz < res[2]; ++iz) {
			for (int iy = 0; iy < res[1]; ++iy) {
				for (int ix = 0; ix < res[0]; ++ix) {
					int ix0 = std::max(0, ix - 1);
					int ix1 = std::min(ix + 1, res[0] - 1);
					int iy0 = std::max(0, iy - 1);
					int iy1 = std::min(iy + 1, res[1] - 1);
					int iz0 = std::max(0, iz - 1);
					int iz1 = std::min(iz + 1, res[2] - 1);

					int linear_x0 = (iz * res[1] + iy) * res[0] + ix0;
					int linear_x1 = (iz * res[1] + iy) * res[0] + ix1;
					int linear_y0 = (iz * res[1] + iy0) * res[0] + ix;
					int linear_y1 = (iz * res[1] + iy1) * res[0] + ix;
					int linear_z0 = (iz0 * res[1] + iy) * res[0] + ix;
					int linear_z1 = (iz1 * res[1] + iy) * res[0] + ix;

					Eigen::Vector3d vel_x0(velocityArray->GetTuple3(linear_x0));
					Eigen::Vector3d vel_x1(velocityArray->GetTuple3(linear_x1));
					Eigen::Vector3d vel_y0(velocityArray->GetTuple3(linear_y0));
					Eigen::Vector3d vel_y1(velocityArray->GetTuple3(linear_y1));
					Eigen::Vector3d vel_z0(velocityArray->GetTuple3(linear_z0));
					Eigen::Vector3d vel_z1(velocityArray->GetTuple3(linear_z1));

					double spacing_x = (ix1 - ix0) * velocityImage->GetSpacing()[0];
					double spacing_y = (iy1 - iy0) * velocityImage->GetSpacing()[1];
					double spacing_z = (iz1 - iz0) * velocityImage->GetSpacing()[2];

					Eigen::Vector3d dv_dx = (vel_x1 - vel_x0) / spacing_x;
					Eigen::Vector3d dv_dy = (vel_y1 - vel_y0) / spacing_y;
					Eigen::Vector3d dv_dz = (vel_z1 - vel_z0) / spacing_z;

					Eigen::Vector3d vorticity(
						dv_dy[2] - dv_dz[1],
						dv_dz[0] - dv_dx[2],
						dv_dx[1] - dv_dy[0]
					);
					int linear = (iz * res[1] + iy) * res[0] + ix;
					vorticityArray->SetTuple1(linear, vorticity.norm());
				}
			}
		}

		// write the file
		AmiraWriter::WriteScalarField(vorticityPath, "vorticity", vorticityImage);
	}

	vtkSmartPointer<vtkImageData> Vorticity::Compute(const vtkSmartPointer<vtkImageData>& velocityField)
	{
		// read the input file
		vtkFloatArray* velocityArray = dynamic_cast<vtkFloatArray*>(velocityField->GetPointData()->GetArray("velocity"));

		// allocate output
		vtkNew<vtkImageData> vorticityImage;
		vorticityImage->SetDimensions(velocityField->GetDimensions());
		vorticityImage->SetOrigin(velocityField->GetOrigin());
		vorticityImage->SetSpacing(velocityField->GetSpacing());
		vtkNew<vtkFloatArray> vorticityArray;
		int64_t numPoints = (int64_t)vorticityImage->GetDimensions()[0]
			* vorticityImage->GetDimensions()[1]
			* vorticityImage->GetDimensions()[2];
		vorticityArray->SetNumberOfTuples(numPoints);
		vorticityArray->SetNumberOfComponents(1);
		vorticityArray->SetName("vorticity");
		vorticityImage->GetPointData()->AddArray(vorticityArray);

		// compute the vorticity field
		int* res = vorticityImage->GetDimensions();
		for (int iz = 0; iz < res[2]; ++iz) {
			for (int iy = 0; iy < res[1]; ++iy) {
				for (int ix = 0; ix < res[0]; ++ix) {
					int ix0 = std::max(0, ix - 1);
					int ix1 = std::min(ix + 1, res[0] - 1);
					int iy0 = std::max(0, iy - 1);
					int iy1 = std::min(iy + 1, res[1] - 1);
					int iz0 = std::max(0, iz - 1);
					int iz1 = std::min(iz + 1, res[2] - 1);

					int linear_x0 = (iz * res[1] + iy) * res[0] + ix0;
					int linear_x1 = (iz * res[1] + iy) * res[0] + ix1;
					int linear_y0 = (iz * res[1] + iy0) * res[0] + ix;
					int linear_y1 = (iz * res[1] + iy1) * res[0] + ix;
					int linear_z0 = (iz0 * res[1] + iy) * res[0] + ix;
					int linear_z1 = (iz1 * res[1] + iy) * res[0] + ix;

					Eigen::Vector3d vel_x0(velocityArray->GetTuple3(linear_x0));
					Eigen::Vector3d vel_x1(velocityArray->GetTuple3(linear_x1));
					Eigen::Vector3d vel_y0(velocityArray->GetTuple3(linear_y0));
					Eigen::Vector3d vel_y1(velocityArray->GetTuple3(linear_y1));
					Eigen::Vector3d vel_z0(velocityArray->GetTuple3(linear_z0));
					Eigen::Vector3d vel_z1(velocityArray->GetTuple3(linear_z1));

					double spacing_x = (ix1 - ix0) * velocityField->GetSpacing()[0];
					double spacing_y = (iy1 - iy0) * velocityField->GetSpacing()[1];
					double spacing_z = (iz1 - iz0) * velocityField->GetSpacing()[2];

					Eigen::Vector3d dv_dx = (vel_x1 - vel_x0) / spacing_x;
					Eigen::Vector3d dv_dy = (vel_y1 - vel_y0) / spacing_y;
					Eigen::Vector3d dv_dz = (vel_z1 - vel_z0) / spacing_z;

					Eigen::Vector3d vorticity(
						dv_dy[2] - dv_dz[1],
						dv_dz[0] - dv_dx[2],
						dv_dx[1] - dv_dy[0]
					);
					int linear = (iz * res[1] + iy) * res[0] + ix;
					vorticityArray->SetTuple1(linear, vorticity.norm());
				}
			}
		}
		return vorticityImage;
	}
}