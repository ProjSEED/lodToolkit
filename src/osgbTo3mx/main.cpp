#include "cmdparser.hpp"
#include "core.h"
#include "osgTo3mx.h"

void configure_parser(cli::Parser& parser) {
	parser.set_required<std::string>("i", "input", "input dir path");
	parser.set_required<std::string>("o", "output", "output dir path");
}

int main(int argc, char** argv)
{
	cli::Parser parser(argc, argv);
	configure_parser(parser);
	parser.run_and_exit_if_error();

	seed::log::DumpLog(seed::log::Info, "Process started...");
	seed::io::OsgTo3mx osgTo3mx;
	if (osgTo3mx.Convert(parser.get<std::string>("i"), parser.get<std::string>("o")))
	{
		seed::log::DumpLog(seed::log::Info, "Process succeed!");
	}
	else
	{
		seed::log::DumpLog(seed::log::Critical, "Process failed!");
	}

	return 1;	
}