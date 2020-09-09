#pragma once

#include "PointVisitor.h"

class TiXmlNode;

namespace seed
{
	namespace io
	{
		class PointVisitor;

		class PointsToOSG
		{
		public:
			///////////////////////////////////////
			// constructors and destructor

			PointsToOSG(std::shared_ptr<PointVisitor> i_oPointVisitor,
				int i_nTileSize, int i_nMaxPointNumPerOneNode, int i_nMaxTreeDepth,
				float i_dLodRatio, float i_fPointSize);

			~PointsToOSG();
			
			///////////////////////////////////////
			// public member functions

			int Write(const std::string& i_cFilePath, std::string i_strColorMode);

		private:
			///////////////////////////////////////
			// private member functions

			bool LoadPointsForOneTile(std::shared_ptr<PointVisitor> i_oPointVisitor,
				std::vector<OSGBPoint>& i_lstPoints);

			int ExportSRS(const std::string& i_cFilePath);


			///////////////////////////////////////
			// private variables

			size_t m_nTileSize;
			size_t m_nProcessedPoints;
			unsigned int m_nMaxTreeDepth;
			unsigned int m_nMaxPointNumPerOneNode;
			double m_dLodRatio;
			float m_fPointSize;
			ColorMode m_eColorMode;

			std::shared_ptr<PointVisitor> m_oPointVisitor;
		};

	};
};