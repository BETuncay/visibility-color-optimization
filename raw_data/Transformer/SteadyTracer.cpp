#include "SteadyTracer.hpp"
#include <vtkPointData.h>
#include <vtkImageData.h>
#include <vtkFloatArray.h>
#include "AmiraReader.hpp"
#include "Sampling.hpp"
#include <omp.h>

namespace vispro
{
	void SteadyTracer::FilterLinesSize(std::vector<Line>& lines, const unsigned minSize) 
	{
		unsigned offset = 0;
		for (unsigned idx = 0; idx < lines.size(); idx++)
		{
			if (lines[idx].size() < minSize)
			{
				offset++;
			}
			else
			{
				lines[idx - offset] = lines[idx];
			}
		}
		lines.resize(lines.size() - offset);
	}

	void SteadyTracer::FilterLinesLength(std::vector<Line>& lines, const double minLength)
	{
		unsigned offset = 0;
		for (unsigned idx = 0; idx < lines.size(); idx++)
		{
			double length = 0;
			for (unsigned i = 0; i < lines[idx].size() - 1; i++)
			{
				length += (lines[idx][i] - lines[idx][i + 1]).norm();
			}

			if (length < minLength)
			{
				offset++;
			}
			else
			{
				lines[idx - offset] = lines[idx];
			}
		}
		lines.resize(lines.size() - offset);
	}

	void SteadyTracer::FilterIntraLineDistance(std::vector<Line>& lines, const double minLength)
	{
		std::vector<unsigned> removePointID; // construct array of points which need to be removed

		for (Line& line : lines)
		{
			
			double distanceAccumulator = 0.0;
			for (unsigned idx = 1; idx < line.size() - 1; idx++) // dont throw away first and last vertex
			{
				double distance = (line[idx] - line[idx - 1]).norm();
				distanceAccumulator += distance;

				if (distanceAccumulator < minLength)
				{
					removePointID.push_back(idx);
				}
				else
				{
					distanceAccumulator = 0.0;
				}
			}

			// now remove points and resize line
			if (removePointID.size() != 0)
			{
				unsigned int removeIndex = 0;
				for (unsigned idx = 0; idx < line.size(); idx++)
				{
					if (removeIndex < removePointID.size() && idx == removePointID[removeIndex])
					{
						removeIndex++;
					}
					else
					{
						line[idx - removeIndex] = line[idx];
					}
				}
				line.resize(line.size() - removeIndex);
				removePointID.clear();	// reuse previous loop memory
			}
		}	
	}

	// maybe implement grid seeding or sphere packing
	void SteadyTracer::SeedLines(std::vector<Eigen::Vector3d>& particles, std::vector<unsigned>& particleIDs, const unsigned numParticles, const Eigen::AlignedBox3d& bounds)
	{
		for (unsigned i = 0; i < numParticles; i++)
		{
			particles[i] = bounds.sample();
			particleIDs[i] = i;
		}
	}

	void SteadyTracer::Streamline(std::vector<Line>& lines, const vtkSmartPointer<vtkImageData>& velocityField, const Eigen::AlignedBox3d& bounds, const unsigned numParticles, const unsigned numSteps, const double maxLength, const double stepSize, const double normalizeFactor)
	{
		std::vector<Eigen::Vector3d> seedParticles(numParticles);
		std::vector<unsigned> seedIds(numParticles);
		SteadyTracer::SeedLines(seedParticles, seedIds, numParticles, bounds);

		double maxLengthHalf = maxLength / 2.0;
		unsigned numStepsHalf = numSteps / 2.0;
		if (numStepsHalf % 2 != 0)
		{
			numStepsHalf++;
		}

		std::vector<Line> forwardLine = SteadyTracer::TraceLines(seedParticles, seedIds, velocityField, numStepsHalf, numParticles, bounds, maxLengthHalf, stepSize, normalizeFactor);
		std::vector<Line> backwardLine = SteadyTracer::TraceLines(seedParticles, seedIds, velocityField, numStepsHalf, numParticles, bounds, maxLengthHalf , -stepSize, normalizeFactor); // negative stepSize -> already flips list vertex order 

		// combine lines
		lines.resize(numParticles);
		for (unsigned i = 0; i < numParticles; i++)
		{
			unsigned totalLineSize = forwardLine[i].size() + backwardLine[i].size() - 1;
			if (totalLineSize != 0)
			{
				lines[i].resize(totalLineSize);
				std::memcpy(&lines[i][0],							&backwardLine[i][0],	(backwardLine[i].size() - 1)	* sizeof(Eigen::Vector3d));
				std::memcpy(&lines[i][backwardLine[i].size() - 1],	&forwardLine[i][0],		forwardLine[i].size()			* sizeof(Eigen::Vector3d));
			}
		}
	}


	std::vector<Line> SteadyTracer::TraceLines(const std::vector<Eigen::Vector3d>& seedParticles, const std::vector<unsigned>& seedIds, const vtkSmartPointer<vtkImageData>& velocityField, const unsigned numSteps, const unsigned numParticles, const Eigen::AlignedBox3d& bounds, const double maxLength, const double stepSize, const double normalizeFactor)
	{
		std::vector<Eigen::Vector3d> particles = seedParticles;
		std::vector<unsigned> particleIDs = seedIds;
		std::vector<Eigen::Vector3d> particlesHistory;
		std::vector<unsigned> particleIDsHistory;
		std::vector<unsigned> deadParticles;
		std::vector<double> totalPathLengths(numParticles, 0.0);
		unsigned currentNumParticles = numParticles;
		for (int step = 0; step < numSteps; step++)
		{
			// add particles to history
			for (int i = 0; i < currentNumParticles; i++)
			{
				// check for dead particles -> particles outside of the domain / particles in a source
				bool isDead = false;
				if (!bounds.contains(particles[i]))
				{
					isDead = true;
				}

				// if a particle has not moved much from its last spot then kill it
				if (!isDead && particlesHistory.size() >= numParticles)
				{
					unsigned predecessorIndex = getPredecessorParticleIndex(particleIDs[i], particleIDsHistory);
					double diff = (particles[i] - particlesHistory[predecessorIndex]).norm();
					totalPathLengths[particleIDs[i]] += diff;

					if (totalPathLengths[particleIDs[i]] > maxLength || diff < 0.000001 )
					{
						isDead = true;
					}
				}

				if (isDead)
				{
					deadParticles.push_back(particleIDs[i]);
				}
				else
				{
					// add advected particles to particle history
					particlesHistory.push_back(particles[i]);
					particleIDsHistory.push_back(particleIDs[i]);
				}
			}

			// remove dead particles from vectors -> inplace remove by iterating through vector and skipping elements which need to be deleted
			if (!deadParticles.empty())
			{
				unsigned int deadIndex = 0;
				for (unsigned i = 0; i < currentNumParticles; i++)
				{
					if (deadIndex < deadParticles.size() && deadParticles[deadIndex] == particleIDs[i])
					{
						deadIndex++;
						continue;
					}
					else
					{
						particles[i - deadIndex] = particles[i];
						particleIDs[i - deadIndex] = particleIDs[i];
					}
				}
				currentNumParticles = currentNumParticles - deadParticles.size();
				particles.resize(currentNumParticles);
				particleIDs.resize(currentNumParticles);
				deadParticles.clear();
			}

			// advect particles
			Advect(particles, velocityField, stepSize, normalizeFactor);
		}

		// extract lines from particle history
		std::vector<Line> lines(numParticles);
		if (stepSize > 0.0)
		{
			for (unsigned i = 0; i < particlesHistory.size(); i++)
			{
				lines[particleIDsHistory[i]].push_back(particlesHistory[i]);
			}
		}
		else
		{
			for (unsigned i = 0; i < particlesHistory.size(); i++)
			{
				lines[particleIDsHistory[i]].insert(lines[particleIDsHistory[i]].begin(), particlesHistory[i]);
			}
		}

		return lines;
	}

	// find previous particle with the same id
	unsigned SteadyTracer::getPredecessorParticleIndex(unsigned particleID, std::vector<unsigned>& particleIDsHistory)
	{
		for (unsigned j = particleIDsHistory.size() - 1; j >= 0; j--)
		{
			if (particleIDsHistory[j] == particleID)
			{
				return j;
			}
		}
		return 0;
	}

	void SteadyTracer::Advect(std::vector<Eigen::Vector3d>& particles, const vtkSmartPointer<vtkImageData>& velocityField, const double stepSize, const double normalizeFactor)
	{
		unsigned errors = 0;
		int64_t numParticles = (int64_t) particles.size();
		#ifndef _DEBUG
		#pragma omp parallel for
		#endif
		for (int64_t i = 0; i < numParticles; ++i) 
		{
			// numerical integration step
			Eigen::Vector3d& pos = particles[i];
			Eigen::Vector3d k1 = Sampling::LinearSample3(pos, velocityField);
#if 1
			// fourth-order Runge-Kutta
			Eigen::Vector3d k2 = Sampling::LinearSample3(pos + 0.5 * stepSize * k1, velocityField);
			Eigen::Vector3d k3 = Sampling::LinearSample3(pos + 0.5 * stepSize * k2, velocityField);
			Eigen::Vector3d k4 = Sampling::LinearSample3(pos + stepSize * k3,		velocityField);
			Eigen::Vector3d change = (k1 + 2 * k2 + 2 * k3 + k4) / (6.0);
#else
			// explicit euler
			Eigen::Vector3d change = k1;
#endif

			change = change * normalizeFactor * stepSize;
			if (std::isnan(change[0]) || std::isnan(change[1]) || std::isnan(change[2]))
			{
				continue;
			}
			else
			{
				pos += change;
			}
		}
	}
}