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
			bool CollectTiles(const std::string& inputData, const std::string& outputData,
				std::vector<std::pair<std::string, std::string>>& topLevels, std::vector<std::pair<std::string, std::string>>& otherLevels);

			bool ConvertMetadataTo3mx(const std::string& input, const std::string& outputDataRootRelative, const std::string& output);
		};
	}
}