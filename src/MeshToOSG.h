#pragma once

#include "Common.h"

namespace seed
{
	namespace io 
	{
		class MeshToOSG
		{
		public:
			MeshToOSG();

			~MeshToOSG();

			bool Convert(const std::string& input, const std::string& output);

		private:
			int ExportSRS(const std::string& i_cFilePath);

		};
	}
}