#include "tgPointsToOSG.h"
#include "tgPointTileToOSG.h"

//#include "tgPointVisitor.h"

#include "tinyxml.h" 

#include "types.h"

#pragma comment(lib, "../thirdparty/TinyXml/lib/tinyxml.lib")

namespace tg
{
	namespace io
	{

		PointsToOSG::PointsToOSG(std::shared_ptr<PointVisitor> i_oPointVisitor) :
			m_oPointVisitor(i_oPointVisitor),
			m_nTileSize(1e6), m_nProcessedPoints(0),
			m_nMaxTreeDepth(99), m_nMaxPointNumPerOneNode(5e3), m_dLodRatio(8.0), m_fPointSize(0.4f)
		{

		}

		PointsToOSG::~PointsToOSG()
		{

		}

		int PointsToOSG::Write(const char* i_cFilePath)
		{
			//QString l_strModule = ("Generate dense cloud LOD");
			//tg::progress::UpdateProgressLabel(l_strModule);
			//tg::ScopeTimer l_oTimer(l_strModule, tg::log::Info);
			tg::progress::UpdateProgress(1);
			if(tg::utils::CheckOrCreateFolder(i_cFilePath) == false)
			{
				return 0;
			}
			//tg::utils::CleanFolder(i_cFilePath);
			//std::shared_ptr<PointVisitor> l_oPointVisitor(new tg::io::PointVisitor(true));
			//l_oPointVisitor->Reset(true);
			std::vector<tg::PointXYZINormalClassT<IntentType, IntenDim>> l_lstPoints;
			l_lstPoints.reserve(this->m_nTileSize);
			tg::progress::UpdateProgress(10);

			this->m_nProcessedPoints = 0;
			size_t l_nTileID = 0;
			std::string l_strOutPutDirPath(i_cFilePath);
			std::vector<std::string> l_lstTileFiles;
			size_t l_nTileCount = m_oPointVisitor->GetNumOfPoints() / m_nTileSize;
			while (this->LoadPointsForOneTile(m_oPointVisitor, l_lstPoints))
			{
				std::cout << "process" << l_nTileID+1 << "/" << l_nTileCount << std::endl;
				std::shared_ptr<PointTileToOSG> lodGenerator = std::make_shared<PointTileToOSG>();
				lodGenerator->SetParameter(this->m_nMaxTreeDepth, this->m_nMaxPointNumPerOneNode, this->m_dLodRatio);
				lodGenerator->SetPointSize(this->m_fPointSize);

				std::string outPutFileFullName;
				char strBlock[16];
				itoa(l_nTileID, strBlock, 10);
				outPutFileFullName = l_strOutPutDirPath + "\\" + std::string(strBlock);
				if (tg::utils::CheckOrCreateFolder(outPutFileFullName) == false)
				{
					return 0;
				}

				try
				{
					lodGenerator->Generate(&l_lstPoints, outPutFileFullName);
					this->m_nProcessedPoints += l_lstPoints.size();
				}
				catch (...)
				{
					tg::log::DumpLog(tg::log::Critical, "Generate point tiles failed!");
					return 0;
				}

				std::string lodName = std::string(strBlock) + "\\" + "L0_0_tile.osgb";

				std::string fullPath = l_strOutPutDirPath + "\\" + lodName;
				if (tg::utils::FileExists(fullPath))
				{
					l_lstTileFiles.push_back(lodName);
				}

				l_nTileID++;

				double l_dPos = ((double)this->m_nProcessedPoints / (double)m_oPointVisitor->GetNumOfPoints()) * 85.0 + 10.0;
				int l_nPos = (int)l_dPos;
				l_nPos = l_nPos < 10 ? 10 : l_nPos;
				l_nPos = l_nPos > 95 ? 95 : l_nPos;
				tg::progress::UpdateProgress(l_nPos);
			}
			/////////////////////////////////////////////////////////////////////////
			{
				tg::log::DumpLog(tg::log::Debug, "Generate root node...");
				std::string mainName = l_strOutPutDirPath + "/DenseCloud.osgb";
				osg::ref_ptr<osg::MatrixTransform> pRoot = new osg::MatrixTransform();
				//pRoot->setMatrix(osg::Matrix::translate(l_oOffset.x, l_oOffset.y, l_oOffset.z));
				osg::ProxyNode* pProxyNode = new osg::ProxyNode();
				for (int i = 0; i < l_lstTileFiles.size(); i++) {
					pProxyNode->setFileName(i, l_lstTileFiles[i]);
				}
				pRoot->addChild(pProxyNode);
				osg::ref_ptr<osgDB::Options> pOpt = new osgDB::Options("precision=15");
				try
				{
					//Ð´ÈëÎÄ¼þ;
					if (osgDB::writeNodeFile(*pRoot, mainName, pOpt) == false)
					{
						tg::log::DumpLog(tg::log::Critical, "Write node file %s failed!", mainName.c_str());
					}
				}
				catch (...)
				{
					tg::log::DumpLog(tg::log::Critical, "Write node file %s failed!", mainName.c_str());
				}
			}

			/////////////////////////////////////////////////////////////////////////
			std::string xmlName = l_strOutPutDirPath + "/SRS.xml";
			this->ExportOffset(xmlName.c_str());
			return 1;
		}

		bool PointsToOSG::LoadPointsForOneTile(std::shared_ptr<PointVisitor> i_oPointVisitor,
			std::vector<tg::PointXYZINormalClassT<IntentType, IntenDim>>& i_lstPoints)
		{
			i_lstPoints.clear();
			size_t l_nCount = 0;

			size_t l_nTileSize = this->m_nTileSize;

			if (i_oPointVisitor->GetNumOfPoints() - this->m_nProcessedPoints <= 1.5 * this->m_nTileSize)
			{
				l_nTileSize = i_oPointVisitor->GetNumOfPoints() - this->m_nProcessedPoints;
			}

			tg::PointXYZINormalClassT<IntentType, IntenDim> l_oPoint;
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
			AddLeafNode(SRS, "Name", m_oPointVisitor->GetSRSName().c_str());
			//AddLeafNode(SRS, "Name", tg::GeoRefDB::GetInstance()->GetCurrentGeoRef().ToName().c_str());

			TiXmlElement * Offset = new TiXmlElement("Offset");
			SRS->LinkEndChild(Offset);
			auto value_offset = m_oPointVisitor->GetOffset();
			AddLeafNode(Offset, "x", value_offset[0]);
			AddLeafNode(Offset, "y", value_offset[1]);
			AddLeafNode(Offset, "z", value_offset[2]);

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