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

			PointCloudToLOD(int i_nTileSize, int i_nMaxPointNumPerOneNode, int i_nMaxTreeDepth,
				float i_dLodRatio, float i_fPointSize);

			~PointCloudToLOD();
			
			///////////////////////////////////////
			// public member functions

			int Export(const std::string& i_exportMode, const std::string& i_filePathInput, const std::string& i_cFilePathOutput, std::string i_strColorMode);

		private:
			///////////////////////////////////////
			// private member functions

			bool LoadPointsForOneTile(std::shared_ptr<PointVisitor> i_oPointVisitor,
				std::vector<PointCI>& i_lstPoints);

			static bool ExportSRS(const std::string& srs, const std::string& i_cFilePath);


			///////////////////////////////////////
			// private variables

			size_t m_nTileSize;
			size_t m_nProcessedPoints;
			unsigned int m_nMaxTreeDepth;
			unsigned int m_nMaxPointNumPerOneNode;
			double m_dLodRatio;
			float m_fPointSize;

			std::shared_ptr<PointVisitor> m_oPointVisitor;
		};

	};
};