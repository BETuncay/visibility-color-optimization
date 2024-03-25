#pragma once

#include <vector>
#include <Eigen/Eigen>
#include <iostream>
#include <fstream>
#include <string>

typedef std::vector<Eigen::Vector3d> Line;

namespace vispro
{
	class ObjectWriter
	{
	public:
		static void WriteObject(const char* filename, const std::vector<Line>& lines, const std::vector<std::vector<double>>& importance, const std::vector<std::vector<double>>& scalarColor);
		static void WriteObjectFast(const char* filename, const std::vector<Line>& lines, const std::vector<std::vector<double>>& importance, const std::vector<std::vector<double>>& scalarColor);

	private:
	};
}