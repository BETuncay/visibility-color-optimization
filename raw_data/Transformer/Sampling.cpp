#include "Sampling.hpp"
#include <vtkImageData.h>
#include <vtkFloatArray.h>
#include <vtkPointData.h>

namespace vispro
{
	double Sampling::LinearSample1(const Eigen::Vector3d& position, vtkImageData* field) {
		Eigen::Vector3d origin(field->GetOrigin());
		Eigen::Vector3d spacing(field->GetSpacing());
		Eigen::Vector3i dimensions(field->GetDimensions());
		Eigen::Vector3d relative = (position - origin).cwiseQuotient(spacing);
		Eigen::Vector3i sample0 = relative.cast<int>();
		Eigen::Vector3i sample1 = sample0 + Eigen::Vector3i(1, 1, 1);
		sample0 = sample0.cwiseMax(Eigen::Vector3i(0, 0, 0)).cwiseMin(dimensions - Eigen::Vector3i(1, 1, 1));
		sample1 = sample1.cwiseMax(Eigen::Vector3i(0, 0, 0)).cwiseMin(dimensions - Eigen::Vector3i(1, 1, 1));
		Eigen::Vector3d interp = relative - sample0.cast<double>();
		vtkFloatArray* vectors = dynamic_cast<vtkFloatArray*>(field->GetPointData()->GetAbstractArray(0));
		return
			(1 - interp.z()) * (1 - interp.y()) * (1 - interp.x()) * vectors->GetTuple1(((int64_t)sample0.z() * dimensions.y() + sample0.y()) * dimensions.x() + sample0.x())
			+ (1 - interp.z()) * (1 - interp.y()) * (interp.x()) * vectors->GetTuple1(((int64_t)sample0.z() * dimensions.y() + sample0.y()) * dimensions.x() + sample1.x())
			+ (1 - interp.z()) * (interp.y()) * (1 - interp.x()) * vectors->GetTuple1(((int64_t)sample0.z() * dimensions.y() + sample1.y()) * dimensions.x() + sample0.x())
			+ (1 - interp.z()) * (interp.y()) * (interp.x()) * vectors->GetTuple1(((int64_t)sample0.z() * dimensions.y() + sample1.y()) * dimensions.x() + sample1.x())
			+ (interp.z()) * (1 - interp.y()) * (1 - interp.x()) * vectors->GetTuple1(((int64_t)sample1.z() * dimensions.y() + sample0.y()) * dimensions.x() + sample0.x())
			+ (interp.z()) * (1 - interp.y()) * (interp.x()) * vectors->GetTuple1(((int64_t)sample1.z() * dimensions.y() + sample0.y()) * dimensions.x() + sample1.x())
			+ (interp.z()) * (interp.y()) * (1 - interp.x()) * vectors->GetTuple1(((int64_t)sample1.z() * dimensions.y() + sample1.y()) * dimensions.x() + sample0.x())
			+ (interp.z()) * (interp.y()) * (interp.x()) * vectors->GetTuple1(((int64_t)sample1.z() * dimensions.y() + sample1.y()) * dimensions.x() + sample1.x());
	}

	Eigen::Vector3d Sampling::LinearSample3(const Eigen::Vector3d& position, vtkImageData* field) {
		Eigen::Vector3d origin(field->GetOrigin());
		Eigen::Vector3d spacing(field->GetSpacing());
		Eigen::Vector3i dimensions(field->GetDimensions());
		Eigen::Vector3d relative = (position - origin).cwiseQuotient(spacing);
		Eigen::Vector3i sample0 = relative.cast<int>();
		Eigen::Vector3i sample1 = sample0 + Eigen::Vector3i(1, 1, 1);
		sample0 = sample0.cwiseMax(Eigen::Vector3i(0, 0, 0)).cwiseMin(dimensions - Eigen::Vector3i(1, 1, 1));
		sample1 = sample1.cwiseMax(Eigen::Vector3i(0, 0, 0)).cwiseMin(dimensions - Eigen::Vector3i(1, 1, 1));
		Eigen::Vector3d interp = relative - sample0.cast<double>();
		vtkFloatArray* vectors = dynamic_cast<vtkFloatArray*>(field->GetPointData()->GetAbstractArray(0));
		return
			(1 - interp.z()) * (1 - interp.y()) * (1 - interp.x()) * Eigen::Vector3d(vectors->GetTuple3(((int64_t)sample0.z() * dimensions.y() + sample0.y()) * dimensions.x() + sample0.x()))
			+ (1 - interp.z()) * (1 - interp.y()) * (interp.x()) * Eigen::Vector3d(vectors->GetTuple3(((int64_t)sample0.z() * dimensions.y() + sample0.y()) * dimensions.x() + sample1.x()))
			+ (1 - interp.z()) * (interp.y()) * (1 - interp.x()) * Eigen::Vector3d(vectors->GetTuple3(((int64_t)sample0.z() * dimensions.y() + sample1.y()) * dimensions.x() + sample0.x()))
			+ (1 - interp.z()) * (interp.y()) * (interp.x()) * Eigen::Vector3d(vectors->GetTuple3(((int64_t)sample0.z() * dimensions.y() + sample1.y()) * dimensions.x() + sample1.x()))
			+ (interp.z()) * (1 - interp.y()) * (1 - interp.x()) * Eigen::Vector3d(vectors->GetTuple3(((int64_t)sample1.z() * dimensions.y() + sample0.y()) * dimensions.x() + sample0.x()))
			+ (interp.z()) * (1 - interp.y()) * (interp.x()) * Eigen::Vector3d(vectors->GetTuple3(((int64_t)sample1.z() * dimensions.y() + sample0.y()) * dimensions.x() + sample1.x()))
			+ (interp.z()) * (interp.y()) * (1 - interp.x()) * Eigen::Vector3d(vectors->GetTuple3(((int64_t)sample1.z() * dimensions.y() + sample1.y()) * dimensions.x() + sample0.x()))
			+ (interp.z()) * (interp.y()) * (interp.x()) * Eigen::Vector3d(vectors->GetTuple3(((int64_t)sample1.z() * dimensions.y() + sample1.y()) * dimensions.x() + sample1.x()));
	}
}