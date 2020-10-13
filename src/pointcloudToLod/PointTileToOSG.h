#pragma once

#include "pointCI.h"

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

		enum ColorMode
		{
			RGB = 0,
			IntensityGrey = 1,
			IntensityBlueWhiteRed = 2,
			IntensityHeightBlend = 3
		};

		class PointTileToOSG
		{
		public:
			PointTileToOSG(unsigned int maxTreeLevel,
				unsigned int maxPointNumPerOneNode,
				double lodRatio,
				float pointSize, 
				osg::BoundingBox boundingBoxGlobal,
				ColorMode colorMode)
			{
				_maxTreeLevel = maxTreeLevel;
				_maxPointNumPerOneNode = maxPointNumPerOneNode;
				_lodRatio = lodRatio;
				_pointSize = pointSize;
				_boundingBoxGlobal = boundingBoxGlobal;
				_colorMode = colorMode;
				CreateColorBar();
			}

			void CreateColorBar();

			bool Generate(const std::vector<PointCI> *pointSet,
				const std::string& saveFilePath, const std::string& strBlock);

		protected:
			unsigned int _maxTreeLevel;
			unsigned int _maxPointNumPerOneNode;
			double _lodRatio;
			float _pointSize;
			osg::BoundingBox _boundingBoxGlobal;
			ColorMode _colorMode;
			osg::Vec4 _colorBar[256];

			AxisInfo FindMaxAxis(osg::BoundingBox boundingBox);

			bool BuildNode(const std::vector<PointCI> *pointSet,
				std::vector<unsigned int> &pointIndex,
				osg::BoundingBox boundingBox,
				osg::BoundingBox boundingBoxLevel0,
				const std::string& saveFilePath,
				const std::string& strBlock,
				unsigned int level,
				unsigned int childNo);

			osg::Geode *MakeNodeGeode(const std::vector<PointCI> *pointSet,
				std::vector<unsigned int> &pointIndex);
		};

	};
};
