#include "pointcloudToLod.h"
#include "cmdparser.hpp"

void configure_parser(cli::Parser& parser) {
	parser.set_required<std::string>("i", "input", "input file path, <ply/las/laz/xyz>");
	parser.set_required<std::string>("o", "output", "output dir path");
	parser.set_optional<std::string>("m", "mode", "3mx", "output mode, <3mx/osgb>");
	parser.set_optional<float>("r", "lodRatio", 1.f, "lod ratio, how many pixel should 1 meter have in level 0 (the most coarse level)");
	parser.set_optional<int>("t", "tileSize", 1000000, "max number of point in one tile");
	parser.set_optional<int>("n", "nodeSize", 5000, "max number of point in one node");
	parser.set_optional<int>("d", "depth", 99, "max lod tree depth");
	parser.set_optional<float>("p", "pointSize", 10.0f, "point size");
	parser.set_optional<std::string>("c", "colorMode", "iHeightBlend", "[las/lsz format only] <rgb/iGrey/iBlueWhiteRed/iHeightBlend>, iGrey/iBlueWhiteRed/iHeightBlend use intensity from las/laz");
}

int main(int argc, char** argv)
{
	cli::Parser parser(argc, argv);
	configure_parser(parser);
	parser.run_and_exit_if_error();

	seed::log::DumpLog(seed::log::Debug, "Process started...");
	seed::io::PointCloudToLOD pointcloudToLOD(parser.get<int>("t"), parser.get<int>("n"), parser.get<int>("d"),
		parser.get<float>("r"), parser.get<float>("p"));

	if (pointcloudToLOD.Export(parser.get<std::string>("m"), parser.get<std::string>("i"), parser.get<std::string>("o"), parser.get<std::string>("c")) > 0)
	{
		seed::log::DumpLog(seed::log::Debug, "Process succeed!");
	}
	else
	{
		seed::log::DumpLog(seed::log::Critical, "Process failed!");
	}

	return 1;	
}