#include "osgTo3mx.h"

#include <execution>
#include <mutex>

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

			seed::progress::UpdateProgress(0);
			if (!utils::CheckOrCreateFolder(output))
			{
				seed::log::DumpLog(seed::log::Critical, "Create folder %s failed!", output.c_str());
				return false;
			}
			if (!utils::CheckOrCreateFolder(outputData))
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

			osgDB::DirectoryContents fileNames = osgDB::getDirectoryContents(inputData);

			std::vector<std::string> tileIds;
			std::vector<std::string> tileRelativePaths;
			std::vector<osg::BoundingBox> tileBBoxes;

			int processed = 0;
			int percent = -1;

			for each (std::string dir in fileNames)
			{
				if (dir.find(".") != std::string::npos)
					continue;

				std::string output3mxbName = dir + "/" + dir + ".3mxb";
				std::string output3mxb = outputData + output3mxbName;
				osg::BoundingBox bb;
				if (!ConvertTile(inputData, outputData, dir, bb))
				{
					seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", output3mxb.c_str());
					return false;
				}
				if (bb.valid())
				{
					tileIds.push_back(dir);
					tileRelativePaths.push_back(output3mxbName);
					tileBBoxes.push_back(bb);
				}

				{
					processed++;
					int cur = processed * 100 / fileNames.size();
					if (cur > percent)
					{
						seed::progress::UpdateProgress(cur);
						percent = cur;
					}
				}
			}

			if (!Generate3mxbRoot(tileIds, tileRelativePaths, tileBBoxes, outputDataRoot))
			{
				seed::log::DumpLog(seed::log::Critical, "Generate %s failed!", outputDataRoot.c_str());
				return false;
			}
			seed::progress::UpdateProgress(100);
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

		bool OsgTo3mx::ConvertTile(const std::string& inputData, const std::string& outputData, const std::string& tileName, osg::BoundingBox& bb)
		{
			std::string inputTile = inputData + tileName + "/";
			std::string outputTile = outputData + tileName + "/";
			if (!utils::CheckOrCreateFolder(outputTile))
			{
				seed::log::DumpLog(seed::log::Critical, "Create folder %s failed!", outputTile.c_str());
				return false;
			}
			// top level
			{
				std::string inputOsgb = inputTile + tileName + ".osgb";
				std::string output3mxb = outputTile + tileName + ".3mxb";
				osg::ref_ptr<osg::Node> osgNode = osgDB::readNodeFile(inputOsgb);
				if (!ConvertOsgbTo3mxb(osgNode, output3mxb, &bb))
				{
					seed::log::DumpLog(seed::log::Critical, "Convert %s failed!", inputOsgb.c_str());
					return false;
				}
			}
			// all other
			osgDB::DirectoryContents fileNames = osgDB::getDirectoryContents(inputTile);
			osgDB::DirectoryContents osgbFileNames;
			std::vector<int> indices;
			std::vector<int> flags;
			int count = 0;
			for each (std::string file in fileNames)
			{
				std::string ext = osgDB::getLowerCaseFileExtension(file);
				if (ext != "osgb")
					continue;

				std::string baseName = osgDB::getNameLessExtension(file);
				if (baseName == tileName)
					continue;

				osgbFileNames.push_back(file);
				indices.push_back(count);
				flags.push_back(0);
				count++;
			}
			seed::log::DumpLog(seed::log::Debug, "Found %d files in %s...", osgbFileNames.size(), inputTile.c_str());

#if _HAS_CXX17 && !_DEBUG
			std::for_each(std::execution::par_unseq, std::begin(indices), std::end(indices), [&](int i)
#else
			std::for_each(std::begin(indices), std::end(indices), [&](int i)
#endif
				{
					std::string file = osgbFileNames[i];
					std::string baseName = osgDB::getNameLessExtension(file);
					std::string inputOsgb = inputTile + baseName + ".osgb";
					std::string output3mxb = outputTile + baseName + ".3mxb";

					osg::ref_ptr<osg::Node> osgNode = osgDB::readNodeFile(inputOsgb);
					if (!ConvertOsgbTo3mxb(osgNode, output3mxb))
					{
						flags[i] = 1;
						seed::log::DumpLog(seed::log::Critical, "Convert %s failed!", inputOsgb.c_str());
					}
				}
			);
			for (int i = 0; i < flags.size(); ++i)
			{
				if (flags[i])
				{
					return false;
				}
			}
			return true;
		}
	}
}