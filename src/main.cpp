#include "PointsToOSG.h"
#include <iostream>

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::cout << "-------------------------------------------------------\n"
			      << "|  Usage: ToOSGB points_file_path output_path			|\n" 
			      << "|  Support file format: ply las laz xyz               |\n" 
				  << "-------------------------------------------------------\n" 
			      << std::endl;
		return -1;
	}

	std::cout << "processing started ..." << std::endl;
	std::shared_ptr<seed::io::PointVisitor> l_pointVistor(new seed::io::PointVisitor);
	if (!l_pointVistor->ReadFile(argv[1]))
	{
		std::cout << "Read file failed" << std::endl;
		return -1;
	}
		

	seed::io::PointsToOSG l_points2OSG(l_pointVistor);
	l_points2OSG.Write(argv[2]);
	std::cout << "processing ended ..." << std::endl;

	return 1;
}