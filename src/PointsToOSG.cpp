#include "PointsToOSG.h"
#include "PointTileToOSG.h"

#include "tinyxml.h" 

namespace seed
{
	namespace io
	{

		PointsToOSG::PointsToOSG(std::shared_ptr<PointVisitor> i_oPointVisitor,
			int i_nTileSize, int i_nMaxPointNumPerOneNode, int i_nMaxTreeDepth,
			float i_dLodRatio, float i_fPointSize) :
			m_oPointVisitor(i_oPointVisitor),
			m_nProcessedPoints(0),
			m_nTileSize(i_nTileSize), m_nMaxPointNumPerOneNode(i_nMaxPointNumPerOneNode), m_nMaxTreeDepth(i_nMaxTreeDepth),
			m_dLodRatio(i_dLodRatio), m_fPointSize(i_fPointSize)
		{

		}

		PointsToOSG::~PointsToOSG()
		{

		}

		int PointsToOSG::Write(const std::string& i_cFilePath)
		{
			seed::progress::UpdateProgress(1);
			if(seed::utils::CheckOrCreateFolder(i_cFilePath) == false)
			{
				return 0;
			}
			std::string filePathData = i_cFilePath + "/Data";
			if (seed::utils::CheckOrCreateFolder(filePathData) == false)
			{
				return 0;
			}
			std::vector<OSGBPoint> l_lstPoints;
			l_lstPoints.reserve(this->m_nTileSize);
			seed::progress::UpdateProgress(10);

			this->m_nProcessedPoints = 0;
			size_t l_nTileID = 0;
			std::vector<std::string> l_lstTileFiles;
			size_t l_nTileCount = std::round(m_oPointVisitor->GetNumOfPoints() / (double)m_nTileSize);
			osg::BoundingBox boundingBoxGlobal;
			while (this->LoadPointsForOneTile(m_oPointVisitor, l_lstPoints))
			{
				seed::log::DumpLog(seed::log::Debug, "Generate [%d/%d] tile...", l_nTileID + 1, l_nTileCount);
				std::shared_ptr<PointTileToOSG> lodGenerator = std::make_shared<PointTileToOSG>(this->m_nMaxTreeDepth, this->m_nMaxPointNumPerOneNode, this->m_dLodRatio, this->m_fPointSize);
				std::string outPutFileFullName;
				char cBlock[16];
				itoa(l_nTileID, cBlock, 10);
				std::string strBlock(cBlock);
				strBlock = "Tile_+" + strBlock;
				outPutFileFullName = filePathData + "/" + strBlock;
				if (seed::utils::CheckOrCreateFolder(outPutFileFullName) == false)
				{
					return 0;
				}

				try
				{
					lodGenerator->Generate(&l_lstPoints, outPutFileFullName, strBlock, boundingBoxGlobal);
					this->m_nProcessedPoints += l_lstPoints.size();
				}
				catch (...)
				{
					seed::log::DumpLog(seed::log::Critical, "Generate point tiles failed!");
					return 0;
				}

				std::string lodName = strBlock + "/" + strBlock + ".osgb";

				std::string fullPath = filePathData + "/" + lodName;
				if (seed::utils::FileExists(fullPath))
				{
					l_lstTileFiles.push_back("./Data/" + lodName);
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
				std::string mainName = i_cFilePath + "/Root.osgb";
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
			std::string xmlName = i_cFilePath + "/metadata.xml";
			this->ExportSRS(xmlName);
			seed::progress::UpdateProgress(100);
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

		int PointsToOSG::ExportSRS(const std::string& i_cFilePath)
		{
			TiXmlDocument xmlDoc(i_cFilePath.c_str());
			TiXmlDeclaration Declaration("1.0", "utf-8", "");
			xmlDoc.InsertEndChild(Declaration);

			TiXmlElement elmRoot("ModelMetadata");
			elmRoot.SetAttribute("Version", "1");

			TiXmlElement * SRS = new TiXmlElement("SRS");
			elmRoot.LinkEndChild(SRS);
			utils::AddLeafNode(SRS, "WKT", m_oPointVisitor->GetSRSName().c_str());

			utils::AddLeafNode(&elmRoot, "SRSOrigin", "0, 0, 0");

			TiXmlElement * Texture = new TiXmlElement("Texture");
			elmRoot.LinkEndChild(Texture);
			utils::AddLeafNode(Texture, "ColorSource", "Visible");

			xmlDoc.InsertEndChild(elmRoot);
			xmlDoc.SaveFile();
			return 1;
		}
	}
}