#include "pointcloudToLod.h"
#include "tileToLod.h"
#include "c3mx.h"

#include <osgDB/FileNameUtils>

namespace seed
{
	namespace io
	{

		PointCloudToLOD::PointCloudToLOD()
		{
			
		}

		PointCloudToLOD::~PointCloudToLOD()
		{

		}

		bool PointCloudToLOD::Export(const std::string& input, const std::string& output, const std::string& exportMode,
			int tileSize, int maxPointNumPerOneNode, int maxTreeDepth,
			float lodRatio, float pointSize, std::string colorMode)
		{
			// check export mode
			ExportMode eExportMode;
			if (exportMode == "osgb")
			{
				eExportMode = ExportMode::OSGB;
			}
			else if (exportMode == "3mx")
			{
				eExportMode = ExportMode::_3MX;
			}
			else
			{
				seed::log::DumpLog(seed::log::Critical, "Mode %s is NOT support!", exportMode.c_str());
				return false;
			}
			seed::log::DumpLog(seed::log::Info, "Export mode: %s", exportMode.c_str());

			// check color mode
			ColorMode eColorMode;
			if (colorMode == "rgb")
			{
				eColorMode = ColorMode::RGB;
			}
			else if (colorMode == "iGrey")
			{
				eColorMode = ColorMode::IntensityGrey;
			}
			else if (colorMode == "iBlueWhiteRed")
			{
				eColorMode = ColorMode::IntensityBlueWhiteRed;
			}
			else if (colorMode == "iHeightBlend")
			{
				eColorMode = ColorMode::IntensityHeightBlend;
			}
			else
			{
				seed::log::DumpLog(seed::log::Critical, "ColorMode %s is NOT supported now.", colorMode.c_str());
				return false;
			}
			seed::log::DumpLog(seed::log::Info, "Color mode: %s", colorMode.c_str());

			// check input
			std::shared_ptr<PointVisitor> pointVisitor = std::shared_ptr<PointVisitor>(new PointVisitor);
			if (!pointVisitor->PerpareFile(input, eColorMode == ColorMode::IntensityHeightBlend))
			{
				seed::log::DumpLog(seed::log::Critical, "Can NOT open file %s", input);
				return false;
			}
			std::string ext = osgDB::getFileExtensionIncludingDot(input);
			seed::log::DumpLog(seed::log::Info, "Input file format: %s", ext.c_str());

			// check output
			if (osgDB::makeDirectory(output) == false)
			{
				return false;
			}
			std::string filePathData = output + "/Data";
			if (osgDB::makeDirectory(filePathData) == false)
			{
				return false;
			}

			// convert
			std::vector<PointCI> lstPoints;
			lstPoints.reserve(tileSize);

			int processedPoints = 0;
			size_t tileID = 0;
			std::vector<std::string> tileIds;
			std::vector<std::string> tileRelativePaths;
			std::vector<osg::BoundingBox> tileBBoxes;
			seed::progress::UpdateProgress(0, true);
			while (this->LoadPointsForOneTile(pointVisitor, lstPoints, tileSize, processedPoints))
			{
				TileToLOD lodGenerator(maxTreeDepth, maxPointNumPerOneNode, lodRatio, pointSize, pointVisitor->GetBBoxZHistogram(), eColorMode);
				std::string tileName = "Tile_+" + std::to_string(tileID++);
				std::string tilePath = filePathData + "/" + tileName;
				if (osgDB::makeDirectory(tilePath) == false)
				{
					return false;
				}

				osg::BoundingBox box;
				if (!lodGenerator.Generate(&lstPoints, tilePath, tileName, eExportMode, box))
				{
					seed::log::DumpLog(seed::log::Critical, "Generate point tiles %s failed!", tilePath.c_str());
					return false;
				}

				std::string topLevelNodeRelativePath;
				if (eExportMode == ExportMode::OSGB)
				{
					topLevelNodeRelativePath = tileName + "/" + tileName + ".osgb";
				}
				else if (eExportMode == ExportMode::_3MX)
				{
					topLevelNodeRelativePath = tileName + "/" + tileName + ".3mxb";
				}
				else
				{
					seed::log::DumpLog(seed::log::Critical, "Mode %d is NOT support!", eExportMode);
					return false;
				}

				if (osgDB::fileExists(filePathData + "/" + topLevelNodeRelativePath))
				{
					tileIds.push_back(tileName);
					tileRelativePaths.push_back(topLevelNodeRelativePath);
					tileBBoxes.push_back(box);
				}

				processedPoints += lstPoints.size();
				seed::progress::UpdateProgress((size_t)processedPoints * 100LL / pointVisitor->GetNumOfPoints());
			}

			// export root and metadata
			if (eExportMode == ExportMode::OSGB)
			{
				std::string outputRoot = output + "/Root.osgb";
				std::string outputMetadata = output + "/metadata.xml";
				osg::ref_ptr<osg::MatrixTransform> pRoot = new osg::MatrixTransform();
				auto l_oOffset = pointVisitor->GetOffset();
				pRoot->setMatrix(osg::Matrix::translate(l_oOffset.x(), l_oOffset.y(), l_oOffset.z()));
				osg::ProxyNode* pProxyNode = new osg::ProxyNode();
				pProxyNode->setCenter(pointVisitor->GetBBox().center());
				pProxyNode->setRadius(pointVisitor->GetBBox().radius());
				pProxyNode->setLoadingExternalReferenceMode(osg::ProxyNode::LOAD_IMMEDIATELY);
				for (int i = 0; i < tileRelativePaths.size(); i++) {
					pProxyNode->setFileName(i, "./Data/" + tileRelativePaths[i]);
				}
				pRoot->addChild(pProxyNode);
				osg::ref_ptr<osgDB::Options> pOpt = new osgDB::Options("precision=15");
				if (osgDB::writeNodeFile(*pRoot, outputRoot, pOpt) == false)
				{
					seed::log::DumpLog(seed::log::Critical, "Write node file %s failed!", outputRoot.c_str());
				}
				if (!ExportSRS(pointVisitor->GetSRSName(), outputMetadata))
				{
					seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", outputMetadata.c_str());
					return false;
				}
			}
			else if (eExportMode == ExportMode::_3MX)
			{
				std::string output3mx = output + "/Root.3mx";
				std::string outputDataRootRelative = "Data/Root.3mxb";
				std::string outputDataRoot = output + "/" + outputDataRootRelative;
				std::string outputMetadata = output + "/metadata.xml";
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
				if (!Generate3mx(pointVisitor->GetSRSName(), osg::Vec3d(0, 0, 0), pointVisitor->GetOffset(), outputDataRootRelative, output3mx))
				{
					seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", output3mx.c_str());
					return false;
				}
			}
			else
			{
				seed::log::DumpLog(seed::log::Critical, "Mode %d is NOT support!", eExportMode);
				return false;
			}

			seed::progress::UpdateProgress(100);
			return true;
		}

		bool PointCloudToLOD::LoadPointsForOneTile(std::shared_ptr<PointVisitor> pointVisitor,
			std::vector<PointCI>& lstPoints, size_t tileSize, size_t processedPoints)
		{
			lstPoints.clear();
			size_t count = 0;

			if (pointVisitor->GetNumOfPoints() - processedPoints <= 1.5 * tileSize)
			{
				tileSize = pointVisitor->GetNumOfPoints() - processedPoints;
			}

			PointCI point;
			while (count < tileSize)
			{
				int l_nFlag = pointVisitor->NextPoint(point);
				if (l_nFlag > 0)
				{
					lstPoints.push_back(point);
					++count;
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
			if (count > 0)
			{
				return true;
			}
			else
			{
				return false;
			}
		}

		bool PointCloudToLOD::ExportSRS(const std::string& srs, const std::string& filePath)
		{
			std::ofstream outfile(filePath);
			if (outfile.bad())
			{
				seed::log::DumpLog(seed::log::Critical, "Can NOT open file %s!", filePath.c_str());
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
				seed::log::DumpLog(seed::log::Critical, "An error has occurred while writing file %s!", filePath.c_str());
				return false;
			}
			return true;
		}
	}
}