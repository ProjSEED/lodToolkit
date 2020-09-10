#include "PointsToOSG.h"
#include "MeshToOSG.h"
#include "CmdParser/cmdparser.hpp"

void configure_parser(cli::Parser& parser) {
	parser.set_required<std::string>("m", "mode", "<pointcloud/mesh>, mesh mode only support *.obj with group info");
	parser.set_required<std::string>("i", "input", "input file path, pointcloud: <ply/las/laz/xyz>, mesh: <obj>");
	parser.set_required<std::string>("o", "output", "output dir path");
	parser.set_optional<float>("r", "lodRatio", 1.f, "lod ratio, how many pixel should 1 meter have in level 0 (the most coarse level)");
	parser.set_optional<int>("t", "tileSize", 1000000, "[pointcloud mode only] max number of point in one tile");
	parser.set_optional<int>("n", "nodeSize", 5000, "[pointcloud mode only] max number of point in one node");
	parser.set_optional<int>("d", "depth", 99, "[pointcloud mode only] max lod tree depth");
	parser.set_optional<float>("p", "pointSize", 10.0f, "[pointcloud mode only] point size");
	parser.set_optional<std::string>("c", "colorMode", "iGrey", "[pointcloud mode & las/lsz format only] <rgb/iGrey/iBlueWhiteRed/iHeightBlend>, iGrey/iBlueWhiteRed/iHeightBlend use intensity from las/laz");
}

int main(int argc, char** argv)
{
	cli::Parser parser(argc, argv);
	configure_parser(parser);
	parser.run_and_exit_if_error();

	if (parser.get<std::string>("m") == "pointcloud")
	{
		seed::log::DumpLog(seed::log::Debug, "Process started...");
		seed::io::PointsToOSG points2OSG(parser.get<int>("t"), parser.get<int>("n"), parser.get<int>("d"),
			parser.get<float>("r"), parser.get<float>("p"));

		if (points2OSG.Write(parser.get<std::string>("i"), parser.get<std::string>("o"), parser.get<std::string>("c")) > 0)
		{
			seed::log::DumpLog(seed::log::Debug, "Process succeed!");
		}
		else
		{
			seed::log::DumpLog(seed::log::Critical, "Process failed!");
		}
	}
	else if(parser.get<std::string>("m") == "mesh")
	{
		seed::io::MeshToOSG mesh2OSG;
		if (mesh2OSG.Convert(parser.get<std::string>("i"), parser.get<std::string>("o"), parser.get<float>("r")))
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
		seed::log::DumpLog(seed::log::Critical, "Mode %s is NOT supported!", parser.get<std::string>("m").c_str());
	}

	return 1;	
}