#pragma once

#include "Eigen/Eigen"

class vtkImageData;

namespace vispro
{
	// Helper for the trilinear sampling of scalar fields and vector fields.
	class Sampling
	{
	public:
		// Linearly samples a 3D scalar field at a given domain location.
		static double LinearSample1(const Eigen::Vector3d& position, vtkImageData* field);
		// Linearly samples a 3D vector field at a given domain location.
		static Eigen::Vector3d LinearSample3(const Eigen::Vector3d& position, vtkImageData* field);
	};
}