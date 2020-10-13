#include "MeshToOSG.h"
#include "cmdparser.hpp"

void configure_parser(cli::Parser& parser) {
	parser.set_required<std::string>("i", "input", "input file path, <obj>");
	parser.set_required<std::string>("o", "output", "output dir path");
	parser.set_optional<float>("r", "lodRatio", 1.f, "lod ratio, how many pixel should 1 meter have in level 0 (the most coarse level)");
}

int main(int argc, char** argv)
{
	cli::Parser parser(argc, argv);
	configure_parser(parser);
	parser.run_and_exit_if_error();

	seed::io::MeshToOSG mesh2OSG;
	if (mesh2OSG.Convert(parser.get<std::string>("i"), parser.get<std::string>("o"), parser.get<float>("r")))
	{
		seed::log::DumpLog(seed::log::Debug, "Process succeed!");
	}
	else
	{
		seed::log::DumpLog(seed::log::Critical, "Process failed!");
	}

	return 1;	
}