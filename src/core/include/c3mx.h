#pragma once

#include "core.h"

#include <memory>
#include <vector>
#include <string>
#include <assert.h>

#include "osg/BoundingBox"
#include "osg/Node"

namespace seed
{
	namespace io
	{
		bool ConvertOsgbTo3mxb(osg::ref_ptr<osg::Node> osgNode, const std::string& output, osg::BoundingBox* pbb = nullptr);

		bool Generate3mxbRoot(const std::vector<std::string>& tileIds, const std::vector<std::string>& tileRelativePaths, const std::vector<osg::BoundingBox>& tileBBoxes, const std::string& output);

		bool Generate3mxMetadata(const std::string& output);

		bool Generate3mx(const std::string& srs, osg::Vec3d srsOrigin, osg::Vec3d offset, const std::string& outputDataRootRelative, const std::string& output);
	}
}