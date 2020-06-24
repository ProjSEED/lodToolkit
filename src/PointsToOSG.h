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

			PointsToOSG(std::shared_ptr<PointVisitor> i_oPointVisitor);

			~PointsToOSG();

			///////////////////////////////////////
			// public member functions

			int Write(const std::string& i_cFilePath);

		private:
			///////////////////////////////////////
			// private member functions

			bool LoadPointsForOneTile(std::shared_ptr<PointVisitor> i_oPointVisitor,
				std::vector<OSGBPoint>& i_lstPoints);

			int ExportSRS(const std::string& i_cFilePath);
			int AddLeafNode(TiXmlNode* pElmParent, const char* pszNode, const char* pszText);
			int AddLeafNode(TiXmlNode* pElmParent, const char* pszNode, double doubText);
			int AddLeafNode(TiXmlNode* pElmParent, const char* pszNode, int intText);

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