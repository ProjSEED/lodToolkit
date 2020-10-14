#include "pointcloudToLod.h"
#include "tileToLod.h"

namespace seed
{
	namespace io
	{

		PointCloudToLOD::PointCloudToLOD(int i_nTileSize, int i_nMaxPointNumPerOneNode, int i_nMaxTreeDepth,
			float i_dLodRatio, float i_fPointSize) :
			m_nProcessedPoints(0),
			m_nTileSize(i_nTileSize), m_nMaxPointNumPerOneNode(i_nMaxPointNumPerOneNode), m_nMaxTreeDepth(i_nMaxTreeDepth),
			m_dLodRatio(i_dLodRatio), m_fPointSize(i_fPointSize)
		{
			
		}

		PointCloudToLOD::~PointCloudToLOD()
		{

		}

		int PointCloudToLOD::Export(const std::string& i_exportMode, const std::string& i_filePathInput, const std::string& i_cFilePathOutput, std::string i_strColorMode)
		{
			seed::progress::UpdateProgress(1);
			// check color mode
			ColorMode l_eColorMode;
			if (i_strColorMode == "rgb")
			{
				l_eColorMode = ColorMode::RGB;
			}
			else if (i_strColorMode == "iGrey")
			{
				l_eColorMode = ColorMode::IntensityGrey;
			}
			else if (i_strColorMode == "iBlueWhiteRed")
			{
				l_eColorMode = ColorMode::IntensityBlueWhiteRed;
			}
			else if (i_strColorMode == "iHeightBlend")
			{
				l_eColorMode = ColorMode::IntensityHeightBlend;
			}
			else
			{
				seed::log::DumpLog(seed::log::Critical, "ColorMode %s is NOT supported now.", i_strColorMode.c_str());
				return 0;
			}
			// check export mode
			ExportMode l_eExportMode;
			if (i_exportMode == "osgb")
			{
				l_eExportMode = ExportMode::OSGB;
			}
			else if (i_exportMode == "3mx")
			{
				l_eExportMode = ExportMode::_3MX;
			}
			else
			{
				seed::log::DumpLog(seed::log::Critical, "Mode %s is NOT support!", i_exportMode.c_str());
				return 0;
			}

			// check input
			m_oPointVisitor = std::shared_ptr<PointVisitor>(new PointVisitor);
			if (!m_oPointVisitor->PerpareFile(i_filePathInput, l_eColorMode == ColorMode::IntensityHeightBlend))
			{
				seed::log::DumpLog(seed::log::Critical, "Can NOT open file %s", i_filePathInput);
				return 0;
			}

			// check output
			if (seed::utils::CheckOrCreateFolder(i_cFilePathOutput) == false)
			{
				return 0;
			}
			std::string filePathData = i_cFilePathOutput + "/Data";
			if (seed::utils::CheckOrCreateFolder(filePathData) == false)
			{
				return 0;
			}
			std::vector<PointCI> l_lstPoints;
			l_lstPoints.reserve(this->m_nTileSize);
			seed::progress::UpdateProgress(10);

			this->m_nProcessedPoints = 0;
			size_t l_nTileID = 0;
			std::vector<std::string> tileIds;
			std::vector<std::string> tileRelativePaths;
			std::vector<osg::BoundingBox> tileBBoxes;
			size_t l_nTileCount = std::round(m_oPointVisitor->GetNumOfPoints() / (double)m_nTileSize);
			while (this->LoadPointsForOneTile(m_oPointVisitor, l_lstPoints))
			{
				seed::log::DumpLog(seed::log::Debug, "Generate [%d/%d] tile...", l_nTileID + 1, l_nTileCount);
				TileToLOD lodGenerator(this->m_nMaxTreeDepth, this->m_nMaxPointNumPerOneNode, this->m_dLodRatio, this->m_fPointSize, m_oPointVisitor->GetBBoxZHistogram(), l_eColorMode);
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
					std::string lodName;
					osg::BoundingBox box;
					lodGenerator.Generate(&l_lstPoints, outPutFileFullName, strBlock, l_eExportMode, box);
					if (l_eExportMode == ExportMode::OSGB)
					{
						lodName = strBlock + "/" + strBlock + ".osgb";
					}
					else if (l_eExportMode == ExportMode::_3MX)
					{
						lodName = strBlock + "/" + strBlock + ".3mxb";
					}
					else
					{
						seed::log::DumpLog(seed::log::Critical, "Mode %d is NOT support!", l_eExportMode);
						return 0;
					}

					std::string fullPath = filePathData + "/" + lodName;
					if (seed::utils::FileExists(fullPath))
					{
						tileIds.push_back(strBlock);
						tileRelativePaths.push_back(lodName);
						tileBBoxes.push_back(box);
					}

					this->m_nProcessedPoints += l_lstPoints.size();
				}
				catch (...)
				{
					seed::log::DumpLog(seed::log::Critical, "Generate point tiles failed!");
					return 0;
				}

				l_nTileID++;

				double l_dPos = ((double)this->m_nProcessedPoints / (double)m_oPointVisitor->GetNumOfPoints()) * 85.0 + 10.0;
				int l_nPos = (int)l_dPos;
				l_nPos = l_nPos < 10 ? 10 : l_nPos;
				l_nPos = l_nPos > 95 ? 95 : l_nPos;
				seed::progress::UpdateProgress(l_nPos);
			}
			/////////////////////////////////////////////////////////////////////////
			if (l_eExportMode == ExportMode::OSGB)
			{
				seed::log::DumpLog(seed::log::Debug, "Generate root node...");
				std::string mainName = i_cFilePathOutput + "/Root.osgb";
				osg::ref_ptr<osg::MatrixTransform> pRoot = new osg::MatrixTransform();
				auto l_oOffset = m_oPointVisitor->GetOffset();
				pRoot->setMatrix(osg::Matrix::translate(l_oOffset.x(), l_oOffset.y(), l_oOffset.z()));
				osg::ProxyNode* pProxyNode = new osg::ProxyNode();
				pProxyNode->setCenter(m_oPointVisitor->GetBBox().center());
				pProxyNode->setRadius(m_oPointVisitor->GetBBox().radius());
				pProxyNode->setLoadingExternalReferenceMode(osg::ProxyNode::LOAD_IMMEDIATELY);
				for (int i = 0; i < tileRelativePaths.size(); i++) {
					pProxyNode->setFileName(i, "./Data/" + tileRelativePaths[i]);
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
				std::string xmlName = i_cFilePathOutput + "/metadata.xml";
				ExportSRS(m_oPointVisitor->GetSRSName(), xmlName);
			}
			else if (l_eExportMode == ExportMode::_3MX)
			{
				std::string output3mx = i_cFilePathOutput + "/Root.3mx";
				std::string outputDataRootRelative = "Data/Root.3mxb";
				std::string outputDataRoot = i_cFilePathOutput + "/" + outputDataRootRelative;
				std::string outputMetadata = i_cFilePathOutput + "/metadata.xml";
				if (!Generate3mxbRoot(tileIds, tileRelativePaths, tileBBoxes, outputDataRoot))
				{
					seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", outputDataRoot.c_str());
					return false;
				}

				if (!Generate3mxMetadata(outputMetadata))
				{
					seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", outputMetadata.c_str());
					return false;
				}

				if (!Generate3mx(m_oPointVisitor->GetSRSName(), osg::Vec3d(0, 0, 0), m_oPointVisitor->GetOffset(), outputDataRootRelative, output3mx))
				{
					seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", outputMetadata.c_str());
					return false;
				}
			}
			else
			{
				seed::log::DumpLog(seed::log::Critical, "Mode %d is NOT support!", l_eExportMode);
				return 0;
			}

			seed::progress::UpdateProgress(100);
			return 1;
		}

		bool PointCloudToLOD::LoadPointsForOneTile(std::shared_ptr<PointVisitor> i_oPointVisitor,
			std::vector<PointCI>& i_lstPoints)
		{
			i_lstPoints.clear();
			size_t l_nCount = 0;

			size_t l_nTileSize = this->m_nTileSize;

			if (i_oPointVisitor->GetNumOfPoints() - this->m_nProcessedPoints <= 1.5 * this->m_nTileSize)
			{
				l_nTileSize = i_oPointVisitor->GetNumOfPoints() - this->m_nProcessedPoints;
			}

			PointCI l_oPoint;
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

		bool PointCloudToLOD::ExportSRS(const std::string& srs, const std::string& i_cFilePath)
		{
			std::ofstream outfile(i_cFilePath);
			if (outfile.bad())
			{
				seed::log::DumpLog(seed::log::Critical, "Can NOT open file %s!", i_cFilePath.c_str());
				return false;
			}
			outfile << "<?xml version=\"1.0\" encoding=\"utf - 8\"?>\n";
			outfile << "<ModelMetadata version=\"1\">\n";
			outfile << "	<SRS>\n";
			outfile << "		<WKT>" << srs << "</WKT>\n";
			outfile << "	</SRS>\n";
			outfile << "	<SRSOrigin>0, 0, 0</SRSOrigin>\n";
			outfile << "	<Texture>\n";
			outfile << "		<ColorSource>Visible</ColorSource>\n";
			outfile << "	</Texture>\n";
			outfile << "</ModelMetadata>\n";
			if (outfile.bad())
			{
				seed::log::DumpLog(seed::log::Critical, "An error has occurred while writing file %s!", i_cFilePath.c_str());
				return false;
			}
			return true;
		}
	}
}