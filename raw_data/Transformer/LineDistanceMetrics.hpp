#pragma once
#include <Eigen/Eigen>
#include <include/nanoflann.hpp>
#include <omp.h>
#include "ObjectReader.hpp"


enum DistanceMetrics {
	MeanL2,
	rMCPD,
};

static const std::vector<DistanceMetrics> AllMetrics{
	MeanL2,
	rMCPD,
};

namespace vispro
{
	class LineDistanceMetrics
	{
	public:
		static Eigen::MatrixXd Compute_Metric(const Lines5d& lines, const DistanceMetrics metric);
		static Eigen::MatrixXd Compute_Mean_L2_Naive(const Lines5d& lines);
		static Eigen::MatrixXd Compute_rMCPD(const Lines5d& lines);

		static std::string ToString(const DistanceMetrics& metric);
	private:
	};

	struct PointCloud3
	{
		PointCloud3(const Line5d& line) : mLine(line) {}
		const Line5d mLine;

		inline size_t kdtree_get_point_count() const 
		{ 
			return mLine.size();
		}

		// Must return the dim'th component of the idx'th point in the class:
		inline double kdtree_get_pt(const size_t idx, const size_t dim) const
		{
			return mLine[idx][dim];
		}

		// Returns the distance between the vector "p1[0:size-1]" and the data point with index "idx_p2" stored in the class:
		/*
		inline double kdtree_distance(const double* p1, const size_t idx_p2, size_t size) const
		{
			double dist = 0;
			for (size_t i = 0; i < size; ++i) {
				double diff = p1[i] - mLine[idx_p2][i];
				dist += diff * diff;
			}
			return dist;
		}
		*/

		// Optional bounding-box computation: return false to default to a standard bbox computation loop.
		//   Return true if the BBOX was already computed by the class and returned in "bb" so it can be avoided to redo it again.
		//   Look at bb.size() to find out the expected dimensionality (e.g. 2 or 3 for point clouds)
		template <class BBOX>
		bool kdtree_get_bbox(BBOX&) const { return false; }
	};

	struct PointCloud5
	{
		PointCloud5(const Line5d& line) : mLine(line) {}
		const Line5d mLine;

		inline size_t kdtree_get_point_count() const
		{
			return mLine.size();
		}

		// Must return the dim'th component of the idx'th point in the class:
		inline double kdtree_get_pt(const size_t idx, const size_t dim) const
		{
			return mLine[idx][dim];
		}

		// Returns the distance between the vector "p1[0:size-1]" and the data point with index "idx_p2" stored in the class:
		/*
		inline double kdtree_distance(const double* p1, const size_t idx_p2, size_t size) const
		{
			double dist = 0;
			for (size_t i = 0; i < size; ++i) {
				double diff = p1[i] - mLine[idx_p2][i];
				dist += diff * diff;
			}
			return dist;
		}
		*/

		// Optional bounding-box computation: return false to default to a standard bbox computation loop.
		//   Return true if the BBOX was already computed by the class and returned in "bb" so it can be avoided to redo it again.
		//   Look at bb.size() to find out the expected dimensionality (e.g. 2 or 3 for point clouds)
		template <class BBOX>
		bool kdtree_get_bbox(BBOX&) const { return false; }
	};
}