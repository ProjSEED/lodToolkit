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
#include <osgDB/ReaderWriter>
#include <osgDB/WriteFile>
#include <osg/LineWidth>
#include <osg/Point>
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
			PointTileToOSG()
			{
				_maxTreeLevel = 20;
				_maxPointNumPerOneNode = 20000;
				_lodRatio = 6;
				_pointSize = -1;
				//_translate = osg::Matrix::translate(0.0,0.0,0.0);
			}
			PointTileToOSG(unsigned int maxTreeLevel,
				unsigned int maxPointNumPerOneNode,
				double lodRatio)
			{
				_maxTreeLevel = maxTreeLevel;
				_maxPointNumPerOneNode = maxPointNumPerOneNode;
				_lodRatio = lodRatio;
			}

			bool SetParameter(unsigned int maxTreeLevel,
				unsigned int maxPointNumPerOneNode,
				double _lodRatio);

			void SetPointSize(float pointSize)
			{
				_pointSize = pointSize;
			}

			bool Generate(const std::vector<OSGBPoint> *pointSet,
				const std::string saveFilePath, osg::BoundingBox& boundingBoxGlobal);
		protected:
			unsigned int _maxTreeLevel;
			unsigned int _maxPointNumPerOneNode;
			double _lodRatio;
			AxisInfo FindMaxAxis(osg::BoundingBox boundingBox);
			float _pointSize;

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
