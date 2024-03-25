#include "ObjectReader.hpp"


namespace vispro
{

	// function copied form lines.cpp
	Lines5d ObjectReader::ReadObject(const std::string& path)
	{
		static const unsigned OBJ_ZERO_BASED_SHIFT = -1;

		Lines5d lines;

		Eigen::Vector3d last(0, 0, 0);
		std::vector<Eigen::Vector3d> vertices;
		std::vector<float> importances;
		std::vector<float> scalarColors;
		std::ifstream myfile(path);
		if (myfile.is_open())
		{
			std::string line;
			while (myfile.good())
			{
				std::getline(myfile, line);
				if (line.size() < 2) continue;
				if (line[0] == 'v' && line[1] == ' ')	// read in vertex
				{
					Eigen::Vector3d vertex;
					float x, y, z;
					if (sscanf_s(line.c_str(), "v %f %f %f", &x, &y, &z) == 3)
						vertices.emplace_back(x, y, z);
				}
				else if (line[0] == 'v' && line[1] == 't')	// read in tex coords (aka importance and scalarColor)
				{
					float imp, col;
					if (sscanf_s(line.c_str(), "vt %f %f", &imp, &col) == 1)
					{
						col = imp; // if no color scalar is given just use the importance instead
					}
					importances.push_back(imp);
					scalarColors.push_back(col);
				}
				else if (line[0] == 'l') // read in line indices
				{
					std::stringstream stream(line);
					std::string throwaway;
					stream >> throwaway;
					unsigned index;
					Line5d line;
					while (stream >> index)
					{
						float dist = (vertices[index + OBJ_ZERO_BASED_SHIFT] - last).norm();
						if (0.0001f < dist && dist < 999999999.f)
						{
							Eigen::Vector<double, 5> temp;
							temp[0] = vertices[index + OBJ_ZERO_BASED_SHIFT][0];
							temp[1] = vertices[index + OBJ_ZERO_BASED_SHIFT][1];
							temp[2] = vertices[index + OBJ_ZERO_BASED_SHIFT][2];
							temp[3] = importances[index + OBJ_ZERO_BASED_SHIFT];
							temp[4] = scalarColors[index + OBJ_ZERO_BASED_SHIFT];
							line.push_back(temp);

							last = vertices[index + OBJ_ZERO_BASED_SHIFT];
						}
					}
					
					
					lines.push_back(line);
				}
			}
			myfile.close();
		}
		return lines;
	}
}