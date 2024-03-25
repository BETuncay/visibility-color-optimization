#pragma once

#include <vector>
#include <Eigen/Eigen>
#include <vtkSmartPointer.h>

class vtkImageData;
class vtkFloatArray;

typedef std::vector<Eigen::Vector3d> Line;

namespace vispro
{
	class SteadyTracer
	{
	public:
		static void Streamline(std::vector<Line>& lines, const vtkSmartPointer<vtkImageData>& velocityField, const Eigen::AlignedBox3d& bounds, const unsigned numParticles, const unsigned numSteps, const double maxLength, const double stepSize, const double normalizeFactor);
		static void FilterLinesSize(std::vector<Line>& lines, const unsigned minSize);
		static void FilterLinesLength(std::vector<Line>& lines, const double minLength);
		static void FilterIntraLineDistance(std::vector<Line>& lines, const double minLength);

	private:
		static std::vector<Line> TraceLines(const std::vector<Eigen::Vector3d>& seedParticles, const std::vector<unsigned>& seedIds, const vtkSmartPointer<vtkImageData>& velocityField, const unsigned numSteps, const unsigned numParticles, const Eigen::AlignedBox3d& bounds, const double maxLength, const double stepSize, const double normalizeFactor);
		static void Advect(std::vector<Eigen::Vector3d>& particles, const vtkSmartPointer<vtkImageData>& velocityField, const double stepSize, const double normalizeFactor);
		static void SeedLines(std::vector<Eigen::Vector3d>& particles, std::vector<unsigned>& particleIDs, const unsigned numParticles, const Eigen::AlignedBox3d& bounds);
		static unsigned getPredecessorParticleIndex(unsigned particleID, std::vector<unsigned>& particleIDsHistory);

	};
}