#pragma once

#include <memory>
#include <vector>
#include <string>
#include <assert.h>

#include "osg/BoundingBox"
#include "osg/Node"

namespace seed
{
	namespace log
	{
		enum LogType
		{
			Debug = 0,
			Warning = 1,
			Critical = 2,
			Fatal = 3,
			Info = 4
		};

		bool DumpLog(const int&		i_nType,	// log type
			const char*		i_cFormat,
			...);
	}

	namespace progress
	{
		void UpdateProgress(int value);
	}

	namespace utils
	{
		bool CheckOrCreateFolder(const std::string& i_strDir);
		bool FileExists(const std::string& i_strPath);
	}

	namespace io
	{
		bool ConvertOsgbTo3mxb(osg::ref_ptr<osg::Node> osgNode, const std::string& output, osg::BoundingBox* pbb = nullptr);

		bool Generate3mxbRoot(const std::vector<std::string>& tileIds, const std::vector<std::string>& tileRelativePaths, const std::vector<osg::BoundingBox>& tileBBoxes, const std::string& output);

		bool Generate3mxMetadata(const std::string& output);

		bool Generate3mx(const std::string& srs, osg::Vec3d srsOrigin, const std::string& outputDataRootRelative, const std::string& output);
	}
}
