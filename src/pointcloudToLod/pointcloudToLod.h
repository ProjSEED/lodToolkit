#pragma once

#include "pointVisitor.h"

namespace seed
{
	namespace io
	{
		class PointCloudToLOD
		{
		public:
			///////////////////////////////////////
			// constructors and destructor

			PointCloudToLOD();

			~PointCloudToLOD();
			
			///////////////////////////////////////
			// public member functions

			bool Export(const std::string& input, const std::string& output, const std::string& exportMode,
				int tileSize, int maxPointNumPerOneNode, int maxTreeDepth,
				float lodRatio, float pointSize, std::string colorMode);

		private:
			///////////////////////////////////////
			// private member functions

			static bool LoadPointsForOneTile(std::shared_ptr<PointVisitor> pointVisitor,
				std::vector<PointCI>& lstPoints, size_t tileSize, size_t processedPoints);

			static bool ExportSRS(const std::string& srs, const std::string& filePath);
		};

	};
};