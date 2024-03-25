#include "LineDistanceMetrics.hpp"


namespace vispro
{

	Eigen::MatrixXd LineDistanceMetrics::Compute_Metric(const Lines5d& lines, const DistanceMetrics metric)
	{
		switch (metric)
		{
		case MeanL2:
			return LineDistanceMetrics::Compute_Mean_L2_Naive(lines);
		case rMCPD:
			return LineDistanceMetrics::Compute_rMCPD(lines);
		default:
			return Eigen::MatrixXd();
		}
	}

	std::string LineDistanceMetrics::ToString(const DistanceMetrics& metric)
	{
		switch (metric)
		{
		case MeanL2:
			return  "MeanL2";
		case rMCPD:
			return "rMCPD";
		default:
			return "Not Implemented";
		}
	}

	Eigen::MatrixXd LineDistanceMetrics::Compute_Mean_L2_Naive(const Lines5d& lines)
	{
		Eigen::MatrixXd distanceMatrix;
		size_t numLines = lines.size();

		distanceMatrix.resize(numLines, numLines);
		#ifndef _DEBUG
		#pragma omp parallel for
		#endif
		for (int i = 0; i < numLines; ++i)
		{
			// the diagonal is zero
			distanceMatrix(i, i) = 0;

			const Line5d& line_i = lines[i];
			// for every other line j
			for (int j = i + 1; j < numLines; ++j)
			{
				double mean = 0;
				int sum = 0;

				// for each vertex of line i and j
				const Line5d& line_j = lines[j];
				for (size_t iv = 0; iv < std::min(line_i.size(), line_j.size()); ++iv)
				{
					double dist = (line_i[iv] - line_j[iv]).norm();
					mean += dist;
					sum += 1;
				}
				if (sum > 0)
					mean /= sum;
				distanceMatrix(i, j) = mean;
				distanceMatrix(j, i) = mean;
			}
		}
		return distanceMatrix;
	}
	
	// kdtree is just used to find closest point -> could also just linearly search through array but will be slower probably
	Eigen::MatrixXd LineDistanceMetrics::Compute_rMCPD(const Lines5d& lines)
	{
		Eigen::MatrixXd distanceMatrix;

		size_t numLines = lines.size();
		using num_t = double;
		using TVertexType = Eigen::Vector<double, 5>;

		typedef nanoflann::KDTreeSingleIndexAdaptor<
			nanoflann::L2_Simple_Adaptor<num_t, PointCloud5>,
			PointCloud5,
			5,
			int
		> my_kd_tree_t;

		distanceMatrix.resize(numLines, numLines);
		#ifndef _DEBUG
		#pragma omp parallel for
		#endif
		for (int i = 0; i < (int)numLines; ++i)
		{
			// build a kd-tree for line i
			const Line5d& line_i = lines[i];
			PointCloud5 cloud(line_i);
			my_kd_tree_t index(5, cloud, nanoflann::KDTreeSingleIndexAdaptorParams(10));
			index.buildIndex();

			// for every other line j
			for (size_t j = 0; j < numLines; ++j)
			{
				// if i=j continue with dist=0
				if (i == j) 
				{
					distanceMatrix(i, j) = 0;
					continue;
				}

				// for each vertex of line j
				double mean = 0;
				const Line5d& line_j = lines[j];
				for (size_t vj = 0; vj < line_j.size(); ++vj)
				{
					// do a knn search to find closest point in line i
					const size_t num_results = 1;
					size_t ret_index;
					num_t out_dist_sqr;
					nanoflann::KNNResultSet<num_t> resultSet(num_results);
					resultSet.init(&ret_index, &out_dist_sqr);
					const double* ptr = line_j[vj].data();
					index.findNeighbors(resultSet, ptr, nanoflann::SearchParameters());
					mean += std::sqrt(out_dist_sqr);
				}
				mean /= line_j.size();

				// take the minimum if the opposite case has been computed
				distanceMatrix(i, j) = mean;
			}
		}
		// take minimum
		for (size_t i = 0; i < numLines; ++i)
			for (size_t j = i + 1; j < numLines; ++j) {
				double value = std::min(distanceMatrix(i, j), distanceMatrix(j, i));
				distanceMatrix(j, i) = value;
				distanceMatrix(i, j) = value;
			}
		return distanceMatrix;
	}
}