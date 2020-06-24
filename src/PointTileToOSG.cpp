#include "PointTileToOSG.h"

namespace seed
{
	namespace io
	{
		AxisInfo PointTileToOSG::FindMaxAxis(osg::BoundingBox boundingBox)
		{
			AxisInfo maxAxisInfo;
			double xLen = boundingBox.xMax() - boundingBox.xMin();
			double yLen = boundingBox.yMax() - boundingBox.yMin();
			double zLen = boundingBox.zMax() - boundingBox.zMin();
			if (xLen >= yLen && xLen >= zLen)
			{
				maxAxisInfo.aixType = 0;
				maxAxisInfo.max = boundingBox.xMax();
				maxAxisInfo.min = boundingBox.xMin();
			}
			else if (yLen >= xLen && yLen >= zLen)
			{
				maxAxisInfo.aixType = 1;
				maxAxisInfo.max = boundingBox.yMax();
				maxAxisInfo.min = boundingBox.yMin();
			}
			else if (zLen >= xLen && zLen >= yLen)
			{
				maxAxisInfo.aixType = 2;
				maxAxisInfo.max = boundingBox.zMax();
				maxAxisInfo.min = boundingBox.zMin();
			}
			return maxAxisInfo;
		}

		bool PointTileToOSG::Generate(const std::vector<OSGBPoint> *pointSet,
			const std::string saveFilePath, osg::BoundingBox& boundingBoxGlobal)
		{
			std::vector<unsigned int> pointIndex;
			osg::BoundingBox boundingBox;
			for (int i = 0; i < pointSet->size(); i++)
			{
				pointIndex.push_back(i);
				OSGBPoint tmpPoint = pointSet->at(i);
				boundingBox.expandBy(osg::Vec3(tmpPoint.P.X(), tmpPoint.P.Y(), tmpPoint.P.Z()));
			}
			boundingBoxGlobal.expandBy(boundingBox);
			try
			{
				BuildNode(pointSet, pointIndex, boundingBox, saveFilePath, 0, 0);
				pointIndex.swap(std::vector<unsigned int>());
			}
			catch (...)
			{
				seed::log::DumpLog(seed::log::Critical, "KDTree generate error!");
				return false;
			}
			return true;
		}

		osg::Geode *PointTileToOSG::MakeNodeGeode(const std::vector<OSGBPoint> *pointSet,
			std::vector<unsigned int> &pointIndex)
		{
			if (pointIndex.size() <= 0)
			{
				return 0;
			}
			osg::ref_ptr<osg::Geode> geode = new osg::Geode;
			osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry;
			osg::ref_ptr<osg::Vec3Array> pointArray = new osg::Vec3Array;
			osg::ref_ptr<osg::Vec4Array> colorArray = new osg::Vec4Array;
			osg::ref_ptr<osg::StateSet> set = new osg::StateSet;
			osg::ref_ptr<osg::Point> point = new osg::Point;

			for (std::vector<unsigned int>::iterator i = pointIndex.begin(); i != pointIndex.end(); i++)
			{
				OSGBPoint tmpPoint = pointSet->at(*i);
				pointArray->push_back(osg::Vec3(tmpPoint.P.X(), tmpPoint.P.Y(), tmpPoint.P.Z()));
				colorArray->push_back(
					osg::Vec4((float)tmpPoint.I[0] / 255.0f,
					(float)tmpPoint.I[1] / 255.0f,
						(float)tmpPoint.I[2] / 255.0f,
						1));
			}

			if (_pointSize > 0)
			{
				point->setSize(_pointSize);
				set->setMode(GL_POINT_SMOOTH, osg::StateAttribute::ON);
				set->setAttribute(point);
				geometry->setStateSet(set);
			}
			geometry->setVertexArray(pointArray.get());
			geometry->setColorArray(colorArray.get());
			geometry->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::POINTS, 0, pointIndex.size()));
			geometry->setColorBinding(osg::Geometry::BIND_PER_VERTEX);

			geometry->getOrCreateStateSet()->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
			geode->addDrawable(geometry.get());
			return geode.release();
		}

		bool PointTileToOSG::BuildNode(const std::vector<OSGBPoint> *pointSet,
			std::vector<unsigned int> &pointIndex,
			osg::BoundingBox boundingBox,
			const std::string saveFilePath,
			unsigned int level,
			unsigned int childNo)
		{
			osg::ref_ptr<osg::Group> mt(new osg::Group);

			osg::ref_ptr<osg::Geode> nodeGeode;
			AxisInfo maxAxisInfo;
			std::vector<unsigned int> selfPointSetIndex;
			std::vector<unsigned int> leftPointSetIndex;
			std::vector<unsigned int> rightPointSetIndex;
			osg::BoundingBox leftBoundingBox;
			osg::BoundingBox rightBoundingBox;
			std::string saveFileName, leftPageName, rightPageName;
			osg::ref_ptr<osg::PagedLOD>  leftPageNode = new osg::PagedLOD;
			osg::ref_ptr<osg::PagedLOD>  rightPageNode = new osg::PagedLOD;

			char tmpSaveFileName[100];
			sprintf(tmpSaveFileName, "%s%d%s%d%s", "/L", level, "_", childNo, "_tile.osgb");
			saveFileName.assign(tmpSaveFileName);
			saveFileName = saveFilePath + saveFileName;

			if (pointIndex.size() < _maxPointNumPerOneNode || level >= _maxTreeLevel)
			{
				nodeGeode = MakeNodeGeode(pointSet, pointIndex);
				try
				{
					if (osgDB::writeNodeFile(*(nodeGeode.get()), saveFileName, new osgDB::ReaderWriter::Options("precision 20")) == false)
					{
						seed::log::DumpLog(seed::log::Critical, "Write node file %s failed!", saveFileName.c_str());
					}
				}
				catch (...)
				{
					seed::log::DumpLog(seed::log::Critical, "Write node file %s failed!", saveFileName.c_str());
				}
				return true;
			}

			maxAxisInfo = FindMaxAxis(boundingBox);
			double mid = (maxAxisInfo.max + maxAxisInfo.min) / 2;

			rightBoundingBox.init();
			leftBoundingBox.init();

			leftBoundingBox = boundingBox;
			rightBoundingBox = boundingBox;

			if (maxAxisInfo.aixType == X)
			{
				leftBoundingBox._max[0] = mid;
				rightBoundingBox._min[0] = mid;
			}
			else if (maxAxisInfo.aixType == Y)
			{
				leftBoundingBox._max[1] = mid;
				rightBoundingBox._min[1] = mid;
			}
			else
			{
				leftBoundingBox._max[2] = mid;
				rightBoundingBox._min[2] = mid;
			}

			float interval = (float)pointIndex.size() / (float)_maxPointNumPerOneNode;
			int count = -1;
			for (int i = 0; i < pointIndex.size(); i++)
			{
				int tmp = int((float)i / interval);
				if (tmp > count && selfPointSetIndex.size() < _maxPointNumPerOneNode)
				{
					count = tmp;
					selfPointSetIndex.push_back(pointIndex[i]);
				}
				else
				{
					OSGBPoint tmpPoint = pointSet->at(pointIndex[i]);
					if (tmpPoint.P[int(maxAxisInfo.aixType)] > mid)
					{
						rightPointSetIndex.push_back(pointIndex[i]);
					}
					else
					{
						leftPointSetIndex.push_back(pointIndex[i]);
					}
				}
			}
			try
			{
				nodeGeode = MakeNodeGeode(pointSet, selfPointSetIndex);
				selfPointSetIndex.swap(std::vector<unsigned int>());
			}
			catch (...)
			{
				seed::log::DumpLog(seed::log::Critical, "Make self node geode error!", saveFileName.c_str());
				return false;
			}

			char tmpLeftPageName[100], tmpRightPageName[100];
			sprintf(tmpLeftPageName, "%s%d%s%d%s", "L", level + 1, "_", childNo * 2, "_tile.osgb");
			leftPageName.assign(tmpLeftPageName);
			sprintf(tmpRightPageName, "%s%d%s%d%s", "L", level + 1, "_", childNo * 2 + 1, "_tile.osgb");
			rightPageName.assign(tmpRightPageName);
			
			double rangeValue = boundingBox.radius()*_lodRatio;
			leftPageNode->setFileName(0, leftPageName);
			leftPageNode->setRange(0, 0, rangeValue);
			leftPageNode->setCenter(leftBoundingBox.center());
			rightPageNode->setFileName(0, rightPageName);
			rightPageNode->setRange(0, 0, rangeValue);
			rightPageNode->setCenter(rightBoundingBox.center());

			mt->addChild(nodeGeode.get());

			// left
			if (leftPointSetIndex.size())
			{
				BuildNode(pointSet, leftPointSetIndex, leftBoundingBox, saveFilePath, level + 1, childNo * 2);
				leftPointSetIndex.swap(std::vector<unsigned int>());
				mt->addChild(leftPageNode.get());
			}
			// right
			if (rightPointSetIndex.size())
			{
				BuildNode(pointSet, rightPointSetIndex, rightBoundingBox, saveFilePath, level + 1, childNo * 2 + 1);
				rightPointSetIndex.swap(std::vector<unsigned int>());
				mt->addChild(rightPageNode.get());
			}

			try
			{
				if (osgDB::writeNodeFile(*(mt.get()), saveFileName, new osgDB::ReaderWriter::Options("precision 20")) == false)
				{
					seed::log::DumpLog(seed::log::Critical, "Write node file %s failed!", saveFileName.c_str());
				}
			}
			catch (...)
			{
				seed::log::DumpLog(seed::log::Critical, "Write node file %s failed!", saveFileName.c_str());
			}
			selfPointSetIndex.swap(std::vector<unsigned int>());
			return true;
		}
	}
}