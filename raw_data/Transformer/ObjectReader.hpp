#pragma once

#include <vector>
#include <Eigen/Eigen>
#include <iostream>
#include <fstream>
#include <string>


typedef std::vector<Eigen::Vector<double, 5>> Line5d;
typedef std::vector<Line5d> Lines5d;

struct Range {
	Range() : min(DBL_MAX), max(-DBL_MAX) {}
	Range(double min, double max) : min(min), max(max) {}
	double min;
	double max;
};

namespace vispro
{
	class ObjectReader
	{
	public:
		static Lines5d ReadObject(const std::string& path);
	private:
	};
}