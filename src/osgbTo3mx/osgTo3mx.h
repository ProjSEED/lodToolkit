#pragma once

#include "core.h"

#include <osg/BoundingBox>
#include <osg/ref_ptr>
#include <osg/Geode>
#include <osg/Geometry>
#include <osg/PagedLOD>
#include <osg/ProxyNode>
#include <osg/Vec3>
#include <osg/Vec4>
#include <osg/MatrixTransform>
#include <osg/LineWidth>
#include <osg/Point>
#include <osgDB/ReaderWriter>
#include <osgDB/ReadFile>
#include <osgDB/WriteFile>
#include <osgDB/FileUtils>
#include <osgDB/XmlParser>
#include <osgDB/FileNameUtils>

namespace seed
{
	namespace io
	{
		class OsgTo3mx
		{
		public:
			OsgTo3mx() {}

			~OsgTo3mx() {}

			bool Convert(const std::string&input, const std::string& output);

		private:
			bool ConvertMetadataTo3mx(const std::string& input, const std::string& outputDataRootRelative, const std::string& output);
			bool ConvertTile(const std::string& inputData, const std::string& outputData, const std::string& tileName, osg::BoundingBox& bb);
		};
	}
}