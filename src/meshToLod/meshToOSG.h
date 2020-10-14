#pragma once

#include "core.h"

namespace seed
{
	namespace io 
	{
		class MeshToOSG
		{
		public:
			MeshToOSG();

			~MeshToOSG();

			bool Convert(const std::string& input, const std::string& output, float lodRatio);

		private:
			int ExportSRS(const std::string& i_cFilePath);

		};
	}
}