#include "PointsToOSG.h"
#include "CmdParser/cmdparser.hpp"

void configure_parser(cli::Parser& parser) {
	parser.set_required<std::string>("i", "input", "input file path, <ply/las/laz/xyz>");
	parser.set_required<std::string>("o", "output", "output dir path");
	parser.set_optional<int>("t", "tileSize", 1000000, "max number of point in one tile");
	parser.set_optional<int>("n", "nodeSize", 5000, "max number of point in one node");
	parser.set_optional<int>("d", "depth", 99, "max lod tree depth");
	parser.set_optional<float>("r", "lodRatio", 0.5f, "lod ratio");
	parser.set_optional<float>("p", "pointSize", 10.0f, "point size");
}

int main(int argc, char** argv)
{
	cli::Parser parser(argc, argv);
	configure_parser(parser);
	parser.run_and_exit_if_error();

	seed::log::DumpLog(seed::log::Debug, "Process started...");
	std::shared_ptr<seed::io::PointVisitor> pointVistor(new seed::io::PointVisitor);
	if (pointVistor->ReadFile(parser.get<std::string>("i")))
	{
		seed::io::PointsToOSG points2OSG(pointVistor,
			parser.get<int>("t"), parser.get<int>("n"), parser.get<int>("d"),
			parser.get<float>("r"), parser.get<float>("p"));
		if (points2OSG.Write(parser.get<std::string>("o")) > 0)
		{
			seed::log::DumpLog(seed::log::Debug, "Process succeed!");
		}
		else
		{
			seed::log::DumpLog(seed::log::Critical, "Process failed!");
		}
	}
	else
	{
		seed::log::DumpLog(seed::log::Critical, "Can NOT open file %s", parser.get<std::string>("i"));
	}
	return 1;	
}