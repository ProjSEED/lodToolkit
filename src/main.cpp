#include "tgPointsToOSG.h"
#include "types.h"
#include <fstream>
#include "liblas/liblas.hpp"

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::cout << "Usage: las2osg las_file_path output_path" << std::endl;
		return -1;
	}

	std::cout << "processing started ..." << std::endl;
	std::shared_ptr<tg::io::PointVisitor> l_pointVistor(new tg::io::PointVisitor);
	l_pointVistor->ReadLasFile(argv[1]);

	tg::io::PointsToOSG l_points2OSG(l_pointVistor);
	l_points2OSG.Write(argv[2]);
	std::cout << "processing ended ..." << std::endl;

	system("pause");
}