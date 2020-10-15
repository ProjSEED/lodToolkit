#include "osgTo3mx.h"
#include "c3mx.h"

#include <execution>
#include <mutex>

#include <osgDB/ReadFile>
#include <osgDB/FileUtils>
#include <osgDB/XmlParser>
#include <osgDB/FileNameUtils>

namespace seed
{
	namespace io
	{
		bool OsgTo3mx::Convert(const std::string& input, const std::string& output)
		{
			std::string inputMetadata = input + "/metadata.xml";
			std::string inputData = input + "/Data/";

			std::string outputMetadata = output + "/metadata.xml";
			std::string output3mx = output + "/Root.3mx";
			std::string outputData = output + "/Data/";
			std::string outputDataRootRelative = "Data/Root.3mxb";
			std::string outputDataRoot = output + "/" + outputDataRootRelative;

			if (!osgDB::makeDirectory(output))
			{
				seed::log::DumpLog(seed::log::Critical, "Create folder %s failed!", output.c_str());
				return false;
			}
			if (!osgDB::makeDirectory(outputData))
			{
				seed::log::DumpLog(seed::log::Critical, "Create folder %s failed!", outputData.c_str());
				return false;
			}

			if (!Generate3mxMetadata(outputMetadata))
			{
				seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", outputMetadata.c_str());
				return false;
			}

			if (!ConvertMetadataTo3mx(inputMetadata, outputDataRootRelative, output3mx))
			{
				seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", output3mx.c_str());
				return false;
			}

			std::vector<std::pair<std::string, std::string>> topLevels;
			std::vector<std::pair<std::string, std::string>> otherLevels;

			if (!CollectTiles(inputData, outputData, topLevels, otherLevels))
			{
				seed::log::DumpLog(seed::log::Critical, "Collect tiles from %s failed!", inputData.c_str());
				return false;
			}
			seed::log::DumpLog(seed::log::Info, "Collect %d tiles, %d nodes!", topLevels.size(), topLevels.size() + otherLevels.size());

			seed::progress::UpdateProgress(0, true);
			int total = topLevels.size() + otherLevels.size();
			int processedTopLevel = 0;
			{
				std::vector<std::string> tileIds;
				std::vector<std::string> tileRelativePaths;
				std::vector<osg::BoundingBox> tileBBoxes;
				for each (auto tileNode in topLevels)
				{
					std::string inputOsgbName = tileNode.first + "/" + tileNode.second + ".osgb";
					std::string output3mxbName = tileNode.first + "/" + tileNode.second + ".3mxb";
					std::string inputOsgb = inputData + inputOsgbName;
					std::string output3mxb = outputData + output3mxbName;

					osg::BoundingBox bb;
					osg::ref_ptr<osg::Node> osgNode = osgDB::readNodeFile(inputOsgb);
					if (!ConvertOsgbTo3mxb(osgNode, output3mxb, &bb))
					{
						seed::log::DumpLog(seed::log::Critical, "Convert %s failed!", inputOsgb.c_str());
						return false;
					}
					if (bb.valid())
					{
						tileIds.push_back(tileNode.first);
						tileRelativePaths.push_back(output3mxbName);
						tileBBoxes.push_back(bb);
					}
					{
						processedTopLevel++;
						int cur = processedTopLevel * 100 / total;
						seed::progress::UpdateProgress(cur);
					}
				}
				if (!Generate3mxbRoot(tileIds, tileRelativePaths, tileBBoxes, outputDataRoot))
				{
					seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", outputDataRoot.c_str());
					return false;
				}
			}

			std::atomic_bool flag = true;
			std::atomic_int processed = processedTopLevel;
#if _HAS_CXX17 && !_DEBUG
			std::for_each(std::execution::par_unseq, std::begin(otherLevels), std::end(otherLevels), [&](std::pair<std::string, std::string> tileNode)
#else
			std::for_each(std::begin(otherLevels), std::end(otherLevels), [&](std::pair<std::string, std::string> tileNode)
#endif
				{
					std::string inputOsgbName = tileNode.first + "/" + tileNode.second + ".osgb";
					std::string output3mxbName = tileNode.first + "/" + tileNode.second + ".3mxb";
					std::string inputOsgb = inputData + inputOsgbName;
					std::string output3mxb = outputData + output3mxbName;

					osg::ref_ptr<osg::Node> osgNode = osgDB::readNodeFile(inputOsgb);
					if (!ConvertOsgbTo3mxb(osgNode, output3mxb))
					{
						seed::log::DumpLog(seed::log::Critical, "Convert %s failed!", inputOsgb.c_str());
						flag.store(false);
					}
					{
						int cur = (processed.fetch_add(1) + 1) * 100 / total;
						seed::progress::UpdateProgress(cur);
					}
				}
			);
			if (!flag)
			{
				return false;
			}			
			seed::progress::UpdateProgress(100);
			return true;
		}

		bool OsgTo3mx::CollectTiles(const std::string& inputData, const std::string& outputData,
			std::vector<std::pair<std::string, std::string>>& topLevels, std::vector<std::pair<std::string, std::string>>& otherLevels)
		{
			osgDB::DirectoryContents fileNames = osgDB::getDirectoryContents(inputData);
			for each (std::string dir in fileNames)
			{
				if (dir.find(".") != std::string::npos)
					continue;

				std::string inputTile = inputData + dir + "/";
				std::string outputTile = outputData + dir + "/";
				seed::log::DumpLog(seed::log::Info, "Collecting folder %s...", inputTile.c_str());
				if (!osgDB::makeDirectory(outputTile))
				{
					seed::log::DumpLog(seed::log::Critical, "Create folder %s failed!", outputTile.c_str());
					return false;
				}
				// top level
				{
					topLevels.push_back({ dir, dir });
				}
				// all other
				osgDB::DirectoryContents fileNames = osgDB::getDirectoryContents(inputTile);
				for each (std::string file in fileNames)
				{
					std::string ext = osgDB::getLowerCaseFileExtension(file);
					if (ext != "osgb")
						continue;

					std::string baseName = osgDB::getNameLessExtension(file);
					if (baseName == dir)
						continue;

					otherLevels.push_back({ dir, baseName });
				}
			}
			return true;
		}

		bool OsgTo3mx::ConvertMetadataTo3mx(const std::string& input, const std::string& outputDataRootRelative, const std::string& output)
		{
			std::string srs;
			osg::Vec3d srsOrigin(0, 0, 0);
			auto xml = osgDB::readXmlFile(input);
			if (xml)
			{
				for (auto i : xml->children)
				{
					if (i->name == "ModelMetadata")
					{
						for (auto j : i->children)
						{
							if (j->name == "SRS")
							{
								srs = j->contents;
							}
							if (j->name == "SRSOrigin")
							{
								sscanf(j->contents.c_str(), "%lf,%lf,%lf", &srsOrigin._v[0], &srsOrigin._v[1], &srsOrigin._v[2]);
							}
						}
						break;
					}
				}
			}
			else
			{
				seed::log::DumpLog(seed::log::Warning, "Can NOT open file %s!", input.c_str());
			}
			return Generate3mx(srs, srsOrigin, osg::Vec3d(0, 0, 0), outputDataRootRelative, output);
		}
	}
}