#pragma once

#include "Common.h"

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
#include <osgDB/WriteFile>
#include <osgDB/FileUtils>

namespace seed
{
	namespace io
	{

		class PointVisitor;

		enum AxisType
		{
			X = 0,
			Y,
			Z
		};

		struct AxisInfo
		{
			int aixType;
			double max;
			double min;
		};

		class PointTileToOSG
		{
		public:
			PointTileToOSG(unsigned int maxTreeLevel,
				unsigned int maxPointNumPerOneNode,
				double lodRatio,
				float pointSize)
			{
				_maxTreeLevel = maxTreeLevel;
				_maxPointNumPerOneNode = maxPointNumPerOneNode;
				_lodRatio = lodRatio;
				_pointSize = pointSize;
			}

			bool Generate(const std::vector<OSGBPoint> *pointSet,
				const std::string saveFilePath, osg::BoundingBox& boundingBoxGlobal);

		protected:
			unsigned int _maxTreeLevel;
			unsigned int _maxPointNumPerOneNode;
			double _lodRatio;
			float _pointSize;

			AxisInfo FindMaxAxis(osg::BoundingBox boundingBox);

			bool BuildNode(const std::vector<OSGBPoint> *pointSet,
				std::vector<unsigned int> &pointIndex,
				osg::BoundingBox boundingBox,
				const std::string saveFilePath,
				unsigned int level,
				unsigned int childNo);

			osg::Geode *MakeNodeGeode(const std::vector<OSGBPoint> *pointSet,
				std::vector<unsigned int> &pointIndex);
		};

	};
};
