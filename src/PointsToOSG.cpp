#include "PointsToOSG.h"
#include "PointTileToOSG.h"

#include "tinyxml.h" 

namespace seed
{
	namespace io
	{

		PointsToOSG::PointsToOSG(std::shared_ptr<PointVisitor> i_oPointVisitor) :
			m_oPointVisitor(i_oPointVisitor),
			m_nTileSize(1e6), m_nProcessedPoints(0),
			m_nMaxTreeDepth(99), m_nMaxPointNumPerOneNode(5e3), m_dLodRatio(8.0), m_fPointSize(2.0f)
		{

		}

		PointsToOSG::~PointsToOSG()
		{

		}

		int PointsToOSG::Write(const char* i_cFilePath)
		{
			//seed::ScopeTimer l_oTimer("PointsToOSG", seed::log::Info);
			seed::progress::UpdateProgress(1);
			if(seed::utils::CheckOrCreateFolder(i_cFilePath) == false)
			{
				return 0;
			}
			//seed::utils::CleanFolder(i_cFilePath);
			std::vector<OSGBPoint> l_lstPoints;
			l_lstPoints.reserve(this->m_nTileSize);
			seed::progress::UpdateProgress(10);

			this->m_nProcessedPoints = 0;
			size_t l_nTileID = 0;
			std::string l_strOutPutDirPath(i_cFilePath);
			std::vector<std::string> l_lstTileFiles;
			size_t l_nTileCount = std::round(m_oPointVisitor->GetNumOfPoints() / (double)m_nTileSize);
			osg::BoundingBox boundingBoxGlobal;
			while (this->LoadPointsForOneTile(m_oPointVisitor, l_lstPoints))
			{
				std::shared_ptr<PointTileToOSG> lodGenerator = std::make_shared<PointTileToOSG>();
				lodGenerator->SetParameter(this->m_nMaxTreeDepth, this->m_nMaxPointNumPerOneNode, this->m_dLodRatio);
				lodGenerator->SetPointSize(this->m_fPointSize);

				std::string outPutFileFullName;
				char strBlock[16];
				itoa(l_nTileID, strBlock, 10);
				outPutFileFullName = l_strOutPutDirPath + "/" + std::string(strBlock);
				if (seed::utils::CheckOrCreateFolder(outPutFileFullName) == false)
				{
					return 0;
				}

				try
				{
					lodGenerator->Generate(&l_lstPoints, outPutFileFullName, boundingBoxGlobal);
					this->m_nProcessedPoints += l_lstPoints.size();
				}
				catch (...)
				{
					seed::log::DumpLog(seed::log::Critical, "Generate point tiles failed!");
					return 0;
				}

				std::string lodName = std::string(strBlock) + "/" + "L0_0_tile.osgb";

				std::string fullPath = l_strOutPutDirPath + "/" + lodName;
				if (seed::utils::FileExists(fullPath))
				{
					l_lstTileFiles.push_back(lodName);
				}

				l_nTileID++;

				double l_dPos = ((double)this->m_nProcessedPoints / (double)m_oPointVisitor->GetNumOfPoints()) * 85.0 + 10.0;
				int l_nPos = (int)l_dPos;
				l_nPos = l_nPos < 10 ? 10 : l_nPos;
				l_nPos = l_nPos > 95 ? 95 : l_nPos;
				seed::progress::UpdateProgress(l_nPos);
			}
			/////////////////////////////////////////////////////////////////////////
			{
				seed::log::DumpLog(seed::log::Debug, "Generate root node...");
				std::string mainName = l_strOutPutDirPath + "/Root.osgb";
				osg::ref_ptr<osg::MatrixTransform> pRoot = new osg::MatrixTransform();
				auto l_oOffset = m_oPointVisitor->GetOffset();
				pRoot->setMatrix(osg::Matrix::translate(l_oOffset.X(), l_oOffset.Y(), l_oOffset.Z()));
				osg::ProxyNode* pProxyNode = new osg::ProxyNode();
				pProxyNode->setCenter(boundingBoxGlobal.center());
				pProxyNode->setRadius(boundingBoxGlobal.radius());
				pProxyNode->setLoadingExternalReferenceMode(osg::ProxyNode::DEFER_LOADING_TO_DATABASE_PAGER);
				for (int i = 0; i < l_lstTileFiles.size(); i++) {
					pProxyNode->setFileName(i, l_lstTileFiles[i]);
				}
				pRoot->addChild(pProxyNode);
				osg::ref_ptr<osgDB::Options> pOpt = new osgDB::Options("precision=15");
				try
				{
					if (osgDB::writeNodeFile(*pRoot, mainName, pOpt) == false)
					{
						seed::log::DumpLog(seed::log::Critical, "Write node file %s failed!", mainName.c_str());
					}
				}
				catch (...)
				{
					seed::log::DumpLog(seed::log::Critical, "Write node file %s failed!", mainName.c_str());
				}
			}

			/////////////////////////////////////////////////////////////////////////
			std::string xmlName = l_strOutPutDirPath + "/SRS.xml";
			this->ExportOffset(xmlName.c_str());
			return 1;
		}

		bool PointsToOSG::LoadPointsForOneTile(std::shared_ptr<PointVisitor> i_oPointVisitor,
			std::vector<OSGBPoint>& i_lstPoints)
		{
			i_lstPoints.clear();
			size_t l_nCount = 0;

			size_t l_nTileSize = this->m_nTileSize;

			if (i_oPointVisitor->GetNumOfPoints() - this->m_nProcessedPoints <= 1.5 * this->m_nTileSize)
			{
				l_nTileSize = i_oPointVisitor->GetNumOfPoints() - this->m_nProcessedPoints;
			}

			OSGBPoint l_oPoint;
			while (l_nCount < l_nTileSize)
			{
				int l_nFlag = i_oPointVisitor->NextPoint(l_oPoint);
				if (l_nFlag > 0)
				{
					i_lstPoints.push_back(l_oPoint);
					++l_nCount;
					if (l_nFlag == 2)
					{
						break;
					}
				}
				else
				{
					break;
				}
			}
			if (l_nCount > 0)
			{
				return true;
			}
			else
			{
				return false;
			}
		}

		int PointsToOSG::ExportOffset(const char* i_cFilePath)
		{
			TiXmlDocument xmlDoc(i_cFilePath);
			TiXmlDeclaration Declaration("1.0", "utf-8", "");
			xmlDoc.InsertEndChild(Declaration);

			TiXmlElement elmRoot("PointCloud");
			elmRoot.SetAttribute("Version", "1.0");

			TiXmlElement * SRS = new TiXmlElement("SRS");
			elmRoot.LinkEndChild(SRS);
			AddLeafNode(SRS, "WKT", m_oPointVisitor->GetSRSName().c_str());
			//AddLeafNode(SRS, "Name", seed::GeoRefDB::GetInstance()->GetCurrentGeoRef().ToName().c_str());

			TiXmlElement * Offset = new TiXmlElement("Offset");
			SRS->LinkEndChild(Offset);
			//auto value_offset = m_oPointVisitor->GetOffset();
			//AddLeafNode(Offset, "x", value_offset[0]);
			//AddLeafNode(Offset, "y", value_offset[1]);
			//AddLeafNode(Offset, "z", value_offset[2]);
			AddLeafNode(Offset, "x", 0);
			AddLeafNode(Offset, "y", 0);
			AddLeafNode(Offset, "z", 0);

			xmlDoc.InsertEndChild(elmRoot);
			xmlDoc.SaveFile();
			return 1;
		}

		//add leaf node  
		int PointsToOSG::AddLeafNode(TiXmlNode* pElmParent, const char* pszNode, const char* pszText)
		{
			TiXmlElement elmNode(pszNode);
			TiXmlText elmText(pszText);
			if (elmNode.InsertEndChild(elmText) == nullptr) return -1;
			if (pElmParent->InsertEndChild(elmNode) == nullptr) return -1;
			return 1;
		}

		int PointsToOSG::AddLeafNode(TiXmlNode* pElmParent, const char* pszNode, double doubText)
		{
			char pszText[256];
			sprintf(pszText, "%.10f", doubText);
			TiXmlElement elmNode(pszNode);
			TiXmlText elmText(pszText);
			if (elmNode.InsertEndChild(elmText) == nullptr) return -1;
			if (pElmParent->InsertEndChild(elmNode) == nullptr) return -1;
			return 1;
		}

		int PointsToOSG::AddLeafNode(TiXmlNode* pElmParent, const char* pszNode, int intText)
		{
			char pszText[256];
			sprintf(pszText, "%d", intText);
			TiXmlElement elmNode(pszNode);
			TiXmlText elmText(pszText);
			if (elmNode.InsertEndChild(elmText) == nullptr) return -1;
			if (pElmParent->InsertEndChild(elmNode) == nullptr) return -1;
			return 1;
		}

	}
}