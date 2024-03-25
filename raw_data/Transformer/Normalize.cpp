#include "Normalize.hpp"


namespace vispro
{
	vtkSmartPointer<vtkImageData> Normalize::NormalizeField(const vtkSmartPointer<vtkImageData>& velocityField, const NormalizationMethod method)
	{
		// normalize velocity field
		switch (method)
		{
		case NormalizationMethod::None:
			return velocityField;
		case NormalizationMethod::MinMax:
			return Normalize::MinMax(velocityField);
		//case NormalizationMethod::Unit:
		//	return Normalize::Unit(velocityField);
		default:
			return velocityField;
		}
	}

	// returns the magnitude of the longest vector
	double Normalize::InverseMaximumMagnitude(const vtkSmartPointer<vtkImageData>& velocityField)
	{
		vtkFloatArray* velocityArray = dynamic_cast<vtkFloatArray*>(velocityField->GetPointData()->GetArray("velocity"));
		int64_t numPoints = (int64_t)velocityField->GetDimensions()[0] * velocityField->GetDimensions()[1] * velocityField->GetDimensions()[2];
		double maxValue = -DBL_MAX;
		for (int idx = 0; idx < numPoints; ++idx)
		{
			Eigen::Vector3d velocity(velocityArray->GetTuple3(idx));
			double magnitude = velocity.norm();
			maxValue = std::max(magnitude, maxValue);
		}
		return 1.0 / maxValue;
	}

	vtkSmartPointer<vtkImageData> Normalize::MinMax(const vtkSmartPointer<vtkImageData>& velocityField)
	{
		// read the input file
		vtkFloatArray* velocityArray = dynamic_cast<vtkFloatArray*>(velocityField->GetPointData()->GetArray("velocity"));

		// allocate output
		vtkNew<vtkImageData> normalizedImage;
		normalizedImage->SetDimensions(velocityField->GetDimensions());
		normalizedImage->SetOrigin(velocityField->GetOrigin());
		normalizedImage->SetSpacing(velocityField->GetSpacing());

		vtkNew<vtkFloatArray> normalizedArray;
		int64_t numPoints = (int64_t)normalizedImage->GetDimensions()[0] * normalizedImage->GetDimensions()[1] * normalizedImage->GetDimensions()[2];
		normalizedArray->SetNumberOfComponents(3);
		normalizedArray->SetNumberOfTuples(numPoints);
		normalizedArray->SetNumberOfValues(numPoints * 3); // allocates memory
		normalizedArray->SetName("velocity");
		normalizedImage->GetPointData()->AddArray(normalizedArray);

		double maxValue = -DBL_MAX;
		for (int idx = 0; idx < numPoints; ++idx)
		{
			Eigen::Vector3d velocity(velocityArray->GetTuple3(idx));
			double magnitude = velocity.norm();
			maxValue = std::max(magnitude, maxValue);
		}

		double inverse_max = 1 / maxValue;
		for (int idx = 0; idx < numPoints; ++idx)
		{
			Eigen::Vector3d velocity(velocityArray->GetTuple3(idx));
			velocity = velocity * inverse_max;
			normalizedArray->SetTuple3(idx, velocity[0], velocity[1], velocity[2]);
		}

		return normalizedImage;
	}
	
	vtkSmartPointer<vtkImageData> Normalize::Unit(const vtkSmartPointer<vtkImageData>&velocityField)
	{
		// read the input file
		vtkFloatArray* velocityArray = dynamic_cast<vtkFloatArray*>(velocityField->GetPointData()->GetArray("velocity"));

		// allocate output
		vtkNew<vtkImageData> normalizedImage;
		normalizedImage->SetDimensions(velocityField->GetDimensions());
		normalizedImage->SetOrigin(velocityField->GetOrigin());
		normalizedImage->SetSpacing(velocityField->GetSpacing());

		vtkNew<vtkFloatArray> normalizedArray;
		int64_t numPoints = (int64_t)normalizedImage->GetDimensions()[0] * normalizedImage->GetDimensions()[1] * normalizedImage->GetDimensions()[2];
		normalizedArray->SetNumberOfComponents(3);
		normalizedArray->SetNumberOfTuples(numPoints);
		normalizedArray->SetNumberOfValues(numPoints * 3); // allocates memory
		normalizedArray->SetName("velocity");
		normalizedImage->GetPointData()->AddArray(normalizedArray);

		for (int idx = 0; idx < numPoints; ++idx)
		{
			Eigen::Vector3d velocity(velocityArray->GetTuple3(idx));
			double magnitude = velocity.norm();
			velocity = velocity / magnitude;
			normalizedArray->SetTuple3(idx, velocity[0], velocity[1], velocity[2]);
		}
		return normalizedImage;
	}
}