#include "ObjectWriter.hpp"
#include <chrono>
#include <iostream>

namespace vispro
{

	void ObjectWriter::WriteObjectFast(const char* filename, const std::vector<Line>& lines, const std::vector<std::vector<double>>& importance, const std::vector<std::vector<double>>& scalarColor)
	{
		if (lines.size() == 0)
		{
			return;
		}
		int vertex_buffer_size = 100;
		std::vector<std::string> lineTexts(lines.size());
		std::vector<int> lineTextsSize(lines.size());
		for (unsigned i = 0; i < lines.size(); i++)
		{
			lineTexts[i] = std::string(lines[i].size() * 2 * vertex_buffer_size, '\0');
		}

		// first write all vertex data (v and vt) to a vector of strings
		#ifndef _DEBUG
		#pragma omp parallel for
		#endif
		for (int64_t id = 0; id < lines.size(); id++)
		{
			char* lineText = &lineTexts[id][0];
			int ptr = 0;
			for (int64_t point = 0; point < lines[id].size(); point++)
			{
				// write vertex position
				ptr += snprintf(&lineText[ptr], vertex_buffer_size, "v %.6f %.6f %.6f\n", lines[id][point][0], lines[id][point][1], lines[id][point][2]); // use to_char

				// write texture cordinates (importance, scalarColor)
				if (scalarColor.empty())
				{
					ptr += snprintf(&lineText[ptr], vertex_buffer_size, "vt %.6f\n", importance[id][point]);
				}
				else
				{
					ptr += snprintf(&lineText[ptr], vertex_buffer_size, "vt %.6f %.6f\n", importance[id][point], scalarColor[id][point]);
				}
			}
			lineTextsSize[id] = ptr;
		}

		// now write line indices to a vector
		std::vector<unsigned> startIndices(lines.size());
		startIndices[0] = 1;
		for (int64_t i = 1; i < startIndices.size(); i++)
		{
			startIndices[i] = startIndices[i - 1] + lines[i - 1].size();
		}

		int line_buffer_size = 30;
		std::vector<std::string> lineIndices(lines.size());
		std::vector<int> lineIndicesSize(lines.size());
		for (unsigned i = 0; i < lines.size(); i++)
		{
			lineIndices[i] = std::string(lines[i].size() * line_buffer_size, '\0');
		}

		#ifndef _DEBUG
		#pragma omp parallel for
		#endif
		for (int64_t id = 0; id < lines.size(); id++)
		{
			char* lineText = &lineIndices[id][0];
			int ptr = 0;
			
			for (unsigned i = 0; i < lines[id].size(); i++)
			{
				ptr += snprintf(&lineText[ptr], line_buffer_size, "%d ", startIndices[id] + i);
			}
			lineIndicesSize[id] = ptr;
		}

		
		std::ofstream objectfile;
		objectfile.open(filename);
		for (int64_t id = 0; id < lines.size(); id++)
		{
			objectfile.write(&lineTexts[id][0], lineTextsSize[id]);
			
			char buffer[20];
			int size = snprintf(buffer, 20, "g line%d\n",(size_t) id);
			objectfile.write(buffer, size);

			objectfile.write("l ", 2);

			objectfile.write(&lineIndices[id][0], lineIndicesSize[id]);
			objectfile.write("\n", 1);
		}
		objectfile.close();
	}

	void ObjectWriter::WriteObject(const char* filename, const std::vector<Line>& lines, const std::vector<std::vector<double>>& importance, const std::vector<std::vector<double>>& scalarColor)
	{
		std::stringstream out(std::stringstream::out);

		unsigned startIndex = 1;
		unsigned endIndex = 1;

		for (unsigned id = 0; id < lines.size(); id++)
		{
			for (unsigned point = 0; point < lines[id].size(); point++)
			{
				// write vertex position
				char buffer[100];
				int cx = snprintf(buffer, 100, "v %.6f %.6f %.6f\n", lines[id][point][0], lines[id][point][1], lines[id][point][2]); // use to_char
				out << buffer;

				// write texture cordinates (importance, scalarColor)
				if (scalarColor.empty())
				{
					cx = snprintf(buffer, 100, "vt %.6f\n", importance[id][point]);
				}
				else
				{
					cx = snprintf(buffer, 100, "vt %.6f %.6f\n", importance[id][point], scalarColor[id][point]);
				}
				out << buffer;
			}
			char buff[100];
			snprintf(buff, sizeof(buff), "g line%u\n", id);
			std::string buffAsStdStr = buff;
			out << buffAsStdStr;

			endIndex += lines[id].size();
			out << "l ";
			for (unsigned i = startIndex; i < endIndex; i++)
			{
				out << i << " ";
			}
			out << '\n';
			startIndex = endIndex;
		}

		std::ofstream objectfile;
		objectfile.open(filename);
		objectfile << out.str();
		objectfile.close();
	}
}