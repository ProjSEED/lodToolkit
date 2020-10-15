#pragma once

#include "core.h"

#include <osg/BoundingBox>

namespace seed
{
	namespace io
	{
		class OsgTo3mx
		{
		public:
			OsgTo3mx() {}

			~OsgTo3mx() {}

			bool Convert(const std::string&input, const std::string& output);

		private:
			bool ConvertMetadataTo3mx(const std::string& input, const std::string& outputDataRootRelative, const std::string& output);
			bool ConvertTile(const std::string& inputData, const std::string& outputData, const std::string& tileName, osg::BoundingBox& bb);
		};
	}
}