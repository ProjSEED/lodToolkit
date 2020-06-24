#include "PointsToOSG.h"
#include "CmdParser/cmdparser.hpp"

void configure_parser(cli::Parser& parser) {
	parser.set_required<std::string>("i", "input", "input file path");
	parser.set_required<std::string>("o", "output", "output dir path");
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
		seed::io::PointsToOSG points2OSG(pointVistor);
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